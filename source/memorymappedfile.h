#pragma once

#include <QObject>
#include <QString>
#include <QFile>
#include <QMutex>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#endif

namespace QtNetworkRequest
{

    /**
     * @brief Cross-platform memory mapped file wrapper class
     *
     * Features:
     * 1. Thread-safe read/write operations
     * 2. Support for large files (>4GB)
     * 3. Automatic file pre-allocation
     * 4. Cross-platform compatibility
     * 5. Exception safety
     */
    class MemoryMappedFile : public QObject
    {
        Q_OBJECT

    public:
        explicit MemoryMappedFile(QObject *parent = nullptr);
        ~MemoryMappedFile();

        /**
         * @brief Open or create memory mapped file
         * @param filePath File path
         * @param size File size (bytes)
         * @return Success status
         */
        bool open(const QString &filePath, qint64 size);

        /**
         * @brief Close memory mapped file
         */
        void close();

        /**
         * @brief Write data to specified position
         * @param offset File offset
         * @param data Data pointer
         * @param size Data size
         * @return Actual bytes written, -1 indicates failure
         */
        qint64 write(qint64 offset, const char *data, qint64 size);

        /**
         * @brief Read data from specified position
         * @param offset File offset
         * @param data Data buffer
         * @param size Size to read
         * @return Actual bytes read, -1 indicates failure
         */
        qint64 read(qint64 offset, char *data, qint64 size) const;

        /**
         * @brief Flush memory mapping to disk
         * @return Success status
         */
        bool flush();

        /**
         * @brief Check if file is opened
         * @return Whether file is opened
         */
        bool isOpen() const;

        /**
         * @brief Get file size
         * @return File size
         */
        qint64 size() const;

        /**
         * @brief Get last error
         * @return Error message
         */
        QString lastError() const;

        /**
         * @brief Get mapped data pointer
         * @return Pointer to mapped memory region
         */
        void *getMappedData() const;

        /**
         * @brief Write data without locking (for internal use by buffers)
         * @param offset File offset
         * @param data Data pointer
         * @param size Data size
         * @return Actual bytes written
         * @warning This method is not thread-safe and should only be used by coordinated buffers
         */
        qint64 writeUnsafe(qint64 offset, const char *data, qint64 size);

    private:
        Q_DISABLE_COPY(MemoryMappedFile)

#ifdef _WIN32
        HANDLE m_fileHandle;
        HANDLE m_mappingHandle;
        quint8 *m_mappedData;
#else
        int m_fileDescriptor;
        quint8 *m_mappedData;
#endif

        QString m_filePath;
        qint64 m_fileSize;
        mutable QString m_lastError;
        mutable QMutex m_mutex;

        /**
         * @brief Set error message
         * @param error Error message
         */
        void setLastError(const QString &error) const;

        /**
         * @brief Get system error message
         * @return System error message
         */
        QString systemErrorString() const;

        /**
         * @brief Pre-allocate file space
         * @param size File size
         * @return Success status
         */
        bool preallocateFile(qint64 size);
    };

} // namespace QtNetworkRequest
