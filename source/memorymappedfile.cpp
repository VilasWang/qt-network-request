#include "memorymappedfile.h"
#include <QDebug>
#include <QDir>

namespace QtNetworkRequest {

MemoryMappedFile::MemoryMappedFile(QObject *parent)
    : QObject(parent)
#ifdef _WIN32
    , m_fileHandle(INVALID_HANDLE_VALUE)
    , m_mappingHandle(nullptr)
#else
    , m_fileDescriptor(-1)
#endif
    , m_mappedData(nullptr)
    , m_fileSize(0)
{
}

MemoryMappedFile::~MemoryMappedFile()
{
    close();
}

bool MemoryMappedFile::open(const QString &filePath, qint64 size)
{
    QMutexLocker locker(&m_mutex);

    if (isOpen()) {
        close();
    }

    if (size <= 0) {
        setLastError(QString("Parameter error: Invalid file size specified - %1 bytes").arg(size));
        return false;
    }

    m_filePath = filePath;
    m_fileSize = size;

    // Ensure directory exists
    QDir dir = QFileInfo(filePath).absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            setLastError(QString("File system error: Failed to create directory - %1").arg(dir.path()));
            return false;
        }
    }

#ifdef _WIN32
    // Windows implementation
    m_fileHandle = CreateFileW(
        filePath.toStdWString().c_str(),
        GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        nullptr
    );

    if (m_fileHandle == INVALID_HANDLE_VALUE) {
        setLastError(systemErrorString());
        return false;
    }

    // Pre-allocate file space
    if (!preallocateFile(size)) {
        CloseHandle(m_fileHandle);
        m_fileHandle = INVALID_HANDLE_VALUE;
        return false;
    }

    // Create file mapping
    m_mappingHandle = CreateFileMappingW(
        m_fileHandle,
        nullptr,
        PAGE_READWRITE,
        (DWORD)((size >> 32) & 0xFFFFFFFF),
        (DWORD)(size & 0xFFFFFFFF),
        nullptr
    );

    if (m_mappingHandle == nullptr) {
        setLastError(systemErrorString());
        CloseHandle(m_fileHandle);
        m_fileHandle = INVALID_HANDLE_VALUE;
        return false;
    }

    // Map file view
    m_mappedData = static_cast<quint8*>(MapViewOfFile(
        m_mappingHandle,
        FILE_MAP_ALL_ACCESS,
        0,
        0,
        size
    ));

    if (m_mappedData == nullptr) {
        setLastError(systemErrorString());
        CloseHandle(m_mappingHandle);
        m_mappingHandle = nullptr;
        CloseHandle(m_fileHandle);
        m_fileHandle = INVALID_HANDLE_VALUE;
        return false;
    }

#else
    // Linux/macOS implementation
    m_fileDescriptor = ::open(
        filePath.toUtf8().constData(),
        O_RDWR | O_CREAT | O_TRUNC,
        0666
    );

    if (m_fileDescriptor == -1) {
        setLastError(systemErrorString());
        return false;
    }

    // Pre-allocate file space
    if (!preallocateFile(size)) {
        ::close(m_fileDescriptor);
        m_fileDescriptor = -1;
        return false;
    }

    // Create memory mapping
    m_mappedData = static_cast<quint8*>(mmap(
        nullptr,
        size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        m_fileDescriptor,
        0
    ));

    if (m_mappedData == MAP_FAILED) {
        setLastError(systemErrorString());
        ::close(m_fileDescriptor);
        m_fileDescriptor = -1;
        m_mappedData = nullptr;
        return false;
    }

#endif

    qDebug() << "[MemoryMappedFile] Successfully mapped file:" << filePath << "size:" << size;
    return true;
}

void MemoryMappedFile::close()
{
    QMutexLocker locker(&m_mutex);

    if (!isOpen()) {
        return;
    }

#ifdef _WIN32
    if (m_mappedData != nullptr) {
        UnmapViewOfFile(m_mappedData);
        m_mappedData = nullptr;
    }

    if (m_mappingHandle != nullptr) {
        CloseHandle(m_mappingHandle);
        m_mappingHandle = nullptr;
    }

    if (m_fileHandle != INVALID_HANDLE_VALUE) {
        CloseHandle(m_fileHandle);
        m_fileHandle = INVALID_HANDLE_VALUE;
    }
#else
    if (m_mappedData != nullptr && m_mappedData != MAP_FAILED) {
        munmap(m_mappedData, m_fileSize);
        m_mappedData = nullptr;
    }

    if (m_fileDescriptor != -1) {
        ::close(m_fileDescriptor);
        m_fileDescriptor = -1;
    }
#endif

    m_filePath.clear();
    m_fileSize = 0;
    m_lastError.clear();

    qDebug() << "[MemoryMappedFile] File closed";
}

qint64 MemoryMappedFile::write(qint64 offset, const char *data, qint64 size)
{
    QMutexLocker locker(&m_mutex);

    if (!isOpen() || m_mappedData == nullptr) {
        setLastError(QString("File operation error: File is not open or has been closed"));
        return -1;
    }

    if (offset < 0 || offset >= m_fileSize) {
        setLastError(QString("Parameter error: Invalid file offset specified - %1").arg(offset));
        return -1;
    }

    if (size <= 0) {
        return 0;
    }

    // Check if exceeding file range
    if (offset + size > m_fileSize) {
        size = m_fileSize - offset;
    }

    // Write directly to memory mapped area
    memcpy(m_mappedData + offset, data, size);

    return size;
}

qint64 MemoryMappedFile::read(qint64 offset, char *data, qint64 size) const
{
    QMutexLocker locker(&m_mutex);

    if (!isOpen() || m_mappedData == nullptr) {
        setLastError(QString("File operation error: File is not open or has been closed"));
        return -1;
    }

    if (offset < 0 || offset >= m_fileSize) {
        setLastError(QString("Parameter error: Invalid file offset specified - %1").arg(offset));
        return -1;
    }

    if (size <= 0) {
        return 0;
    }

    // Check if exceeding file range
    if (offset + size > m_fileSize) {
        size = m_fileSize - offset;
    }

    // Read directly from memory mapped area
    memcpy(data, m_mappedData + offset, size);

    return size;
}

bool MemoryMappedFile::flush()
{
    QMutexLocker locker(&m_mutex);

    if (!isOpen() || m_mappedData == nullptr) {
        setLastError(QString("File operation error: File is not open or has been closed"));
        return false;
    }

#ifdef _WIN32
    // Windows syncs automatically, but we can force flush
    return FlushViewOfFile(m_mappedData, m_fileSize) != FALSE;
#else
    // Linux/macOS sync to disk
    return msync(m_mappedData, m_fileSize, MS_SYNC) == 0;
#endif
}

bool MemoryMappedFile::isOpen() const
{
    return m_mappedData != nullptr;
}

qint64 MemoryMappedFile::size() const
{
    return m_fileSize;
}

QString MemoryMappedFile::lastError() const
{
    return m_lastError;
}

void* MemoryMappedFile::getMappedData() const
{
    return m_mappedData;
}

qint64 MemoryMappedFile::writeUnsafe(qint64 offset, const char *data, qint64 size)
{
    // No locking for performance - caller must ensure thread safety
    
    if (!isOpen() || m_mappedData == nullptr) {
        return -1;
    }

    if (offset < 0 || offset >= m_fileSize) {
        return -1;
    }

    if (size <= 0) {
        return 0;
    }

    // Check if exceeding file range
    if (offset + size > m_fileSize) {
        size = m_fileSize - offset;
    }

    // Write directly to memory mapped area (no lock)
    memcpy(m_mappedData + offset, data, size);

    return size;
}

void MemoryMappedFile::setLastError(const QString &error) const
{
    m_lastError = error;
    qWarning() << "[MemoryMappedFile] Error:" << error;
}

QString MemoryMappedFile::systemErrorString() const
{
#ifdef _WIN32
    DWORD errorCode = GetLastError();
    if (errorCode == 0) {
        return QString();
    }

    LPWSTR messageBuffer = nullptr;
    size_t size = FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        errorCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&messageBuffer,
        0,
        nullptr
    );

    QString message;
    if (size > 0 && messageBuffer) {
        message = QString::fromWCharArray(messageBuffer, size);
        LocalFree(messageBuffer);
    } else {
        message = QString("System error: Unknown error occurred (code: %1)").arg(errorCode);
    }

    return message;
#else
    return QString::fromLocal8Bit(strerror(errno));
#endif
}

bool MemoryMappedFile::preallocateFile(qint64 size)
{
#ifdef _WIN32
    LARGE_INTEGER li;
    li.QuadPart = size;
    if (!SetFilePointerEx(m_fileHandle, li, nullptr, FILE_BEGIN)) {
        setLastError(systemErrorString());
        return false;
    }

    if (!SetEndOfFile(m_fileHandle)) {
        setLastError(systemErrorString());
        return false;
    }

    // Reset file pointer to beginning
    li.QuadPart = 0;
    SetFilePointerEx(m_fileHandle, li, nullptr, FILE_BEGIN);

#else
    // Linux/macOS: Use ftruncate to pre-allocate space
    if (ftruncate(m_fileDescriptor, size) == -1) {
        setLastError(systemErrorString());
        return false;
    }
#endif

    return true;
}

} // namespace QtNetworkRequest