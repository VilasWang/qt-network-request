#ifndef NETWORKREPLY_H
#define NETWORKREPLY_H
#pragma once

#include <QObject>
#include "networkrequestdefs.h"
#include "networkrequestglobal.h"
#include <memory>
#include <QSharedPointer>

class QEvent;
namespace QtNetworkRequest
{
	// Object will be automatically destroyed, do not destroy manually
	class NETWORK_EXPORT NetworkReply : public QObject
	{
		Q_OBJECT

	public:
		NetworkReply(std::unique_ptr<TaskData> task, QObject *parent = 0);
		~NetworkReply();

		bool isBatchRequest() const { return m_task && m_task->batchId > 0; }
		const TaskData* task() const { return m_task.get(); }

		virtual bool event(QEvent *) Q_DECL_OVERRIDE;

Q_SIGNALS:
    void requestFinished(QSharedPointer<QtNetworkRequest::ResponseResult>);
    void downloadProgress(qint64 bytesDownloaded, qint64 bytesTotal);
    void uploadProgress(qint64 bytesUploaded, qint64 bytesTotal);
    void batchDownloadProgress(qint64 bytesDownloaded);
    void batchUploadProgress(qint64 bytesUploaded);

	protected:
		void replyResult(QSharedPointer<QtNetworkRequest::ResponseResult> rsp, bool bDestroy = false);
		friend class NetworkRequestManager;
		friend class NetworkRequestManagerPrivate;

	private:
		std::unique_ptr<TaskData> m_task;
	};
}

#endif // NETWORKREPLY_H