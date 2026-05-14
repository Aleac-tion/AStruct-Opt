//v1.1更新 增加了Aleain云授权接口
//v1.2更新 开放了授权直连接口
//v1.3更新 增加了abort接口
//v1.4对上传和下载函数进行了优化，防止了空指针和数据乱流导致的崩溃

#include "NeaseServer.h"
#include <qdir.h>
#include <QFile>
#include <QTimer>
#include <QDir>
#include <QThread>
#include <QThreadPool>
#include <QRunnable>
NeaseServer::NeaseServer(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    connect(server, &QTcpServer::newConnection, this, &NeaseServer::onNewConnection);
    startServer(1234);
	main->loaddata(AStruct::static_getDir() + "/main.as");
    root_path = QString::fromLocal8Bit(main->getvalue("Main","root","root_path"));
    {
        model->setHeaderData(0, Qt::Horizontal, "ID");
        model->setHeaderData(1, Qt::Horizontal, "password");
        model->setHeaderData(2, Qt::Horizontal, "ip");
        model->setHeaderData(3, Qt::Horizontal, "status");
        ui.tableView->setModel(model);

    }
	
    init_conf();
}

void NeaseServer::init_conf() {
    AList list;
    for (int i = 0; i < main->getKeyCount("Main", "Users"); i++) {
        list = AList::autoParse(main->getvalue("Main", "Users", i));
        QString username = QString::fromStdString(list[0]);
        QString password = QString::fromStdString(list[1]);
        model->appendRow(new QStandardItem(username));
        model->setItem(model->rowCount() - 1, 1, new QStandardItem(password));
        model->setItem(model->rowCount() - 1, 2, new QStandardItem("NULL"));
        model->setItem(model->rowCount() - 1, 3, new QStandardItem("offline"));
    }
}

void NeaseServer::startServer(quint16 port)
{
    if (!server->listen(QHostAddress::Any, port)) {
		ui.textEdit->append(QString("无法启动服务器: %1").arg(server->errorString()));
        return;
    }

	ui.textEdit->append(QString("服务器已启动，等待客户端连接:%1").arg(port));
}

void NeaseServer::incomingConnection(qintptr socketDescriptor)
{
    QTcpSocket* clientSocket = new QTcpSocket(this);
    clientSocket->setSocketDescriptor(socketDescriptor);

    connect(clientSocket, &QTcpSocket::readyRead,
        this, &NeaseServer::onReadyRead);
    connect(clientSocket, &QTcpSocket::disconnected,
        this, &NeaseServer::onClientDisconnected);

    m_clients.append(clientSocket);

	ui.textEdit->append(QString("新客户端连接: %1").arg(clientSocket->peerAddress().toString()));
}

void NeaseServer::onNewConnection()
{
    // 获取 QTcpServer 中等待的连接
    QTcpSocket* clientSocket = server->nextPendingConnection();
    if (!clientSocket) return;

    connect(clientSocket, &QTcpSocket::readyRead,
        this, &NeaseServer::onReadyRead);
    connect(clientSocket, &QTcpSocket::disconnected,
        this, &NeaseServer::onClientDisconnected);

    m_clients.append(clientSocket);

    ui.textEdit->append(QString("新客户端连接: %1").arg(clientSocket->peerAddress().toString()));
}

void NeaseServer::onReadyRead() {
    ui.textEdit->clear();
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    if (socket->state() != QAbstractSocket::ConnectedState) {
        // 已经不是连接状态，不再处理任何数据，防止读到乱流
        return;
    }

    QByteArray data = socket->readAll();
    if (data.isEmpty()) {
        return;
    }

    // 1. 文本命令处理
    QString message = QString::fromUtf8(data).trimmed();
    if (data.startsWith("#cmd/")) {
        const QStringList parts = message.split('/');
        if (parts.size() > 1) {
            if (parts[1] == "RE") {
                auto vec = message.split("/");
                registerUser(vec);
                return;
            }
            else if (parts[1] == "LG") {
                auto vec = message.split("/");
                loggin(vec);
                return;
            }
			else if (parts[1] == "UPL") {
                send_to_client(socket);
                return;
            }
            else if (parts[1] == "RL") {
                auto vec = message.split("/");
				QString filename = vec[3];
				QString Username = vec[2];
				QString path = root_path + "/" + Username + "/files/" + filename;
				ui.textEdit->append(QStringLiteral("收到删除文件命令: %1").arg(path));
				QFile::remove(path);
                send_to_client(socket);
            }
            else if (parts[1] == "DL") {
				auto vec = message.split("/");
				QString filename = vec[3];
				QString Username = vec[2];
				QString path = root_path + "/" + Username + "/files/" + filename;
				//send_file_to_client(socket, path);
                writeToClient(socket, path);
            }
            else if (parts[1] == "TOKEN") {
				ui.textEdit->append(QString("收到TOKEN授权命令%1").arg(parts[2]));
                if (parts[2] == "Aleain") {
                    if (parts[3] == "GetDir") {
						//开始处理获取目录命令
                        QString User = parts[4];
                        QString path = root_path + "/" + User + "/files/Aleain";
                        QDir dir(path);
                        if (!dir.exists()) {
                            dir.mkdir(path);
                            dir.mkdir(path+"/NewFiles");
							process_Aleain(socket, path);

                        }
                        else {
                            process_Aleain(socket, path);
                        }
                    }
                }
                else if (parts[2] == "Aledicater") {
                    if (parts[3] == "init") {
                        auto newparts = parts[4].split("#datas#");
                        QString User = newparts[0];
                        QString path = root_path + "/" + User + "/files/Aledicater";
                        QDir dir(path);
                        QString structdatas = message.split("#datas#").last();
                        AStruct as;
                                     
                        as.singlesaved(structdatas.toStdString(), path.toStdString() + "/config.astruct");
                        
                        
                    }
                }
            }
            else {
                ui.textEdit->append(
                    QStringLiteral("收到未知文本命令: %1").arg(message));
            }
        }
        return;
    }

    // 2. 文件协议处理（头帧 + 数据帧）
    FileReceiveState& state = m_fileStates[socket];
    state.streamBuffer.append(data);
    QByteArray& buffer = state.streamBuffer;

    const quint32 MAGIC = 0x4E46544C; // 与发送端保持一致
    // header0: magic(4) + type(1) + nameLen(4) + fileSize(8) = 17 字节
    const int HEADER0_SIZE = 4 + 1 + 4 + 8;
    // header1: magic(4) + type(1) + chunkSize(4) = 9 字节
    const int HEADER1_SIZE = 4 + 1 + 4;

    while (true) {
        // 至少要能看见 magic + type
        if (buffer.size() < 5) {
            break;
        }

        const uchar* raw = reinterpret_cast<const uchar*>(buffer.constData());
        quint32 magic = qFromBigEndian<quint32>(raw);
        if (magic != MAGIC) {
            // 流不同步，直接清空状态，避免死循环
            ui.textEdit->append(
                QStringLiteral("文件协议 magic 错误，清理当前连接状态"));
            m_fileStates.remove(socket);
            break;
        }

        uchar type = raw[4];

        if (!state.headerReceived) {
            // 期待的是文件头帧
            if (type != 0) {
                ui.textEdit->append(
                    QStringLiteral("未收到文件头却收到了 type=%1，丢弃").arg(type));
                m_fileStates.remove(socket);
                break;
            }

            if (buffer.size() < HEADER0_SIZE) {
                // 头还没收完
                break;
            }

            quint32 nameLen = qFromBigEndian<quint32>(raw + 5);
            quint64 fileSize = qFromBigEndian<quint64>(raw + 9);

            // 检查 nameLen 部分是否完整
            if (buffer.size() < HEADER0_SIZE + static_cast<int>(nameLen)) {
                // 等下次
                break;
            }

            // 丢掉头部
            buffer.remove(0, HEADER0_SIZE);
            QByteArray nameBytes = buffer.left(static_cast<int>(nameLen));
            buffer.remove(0, static_cast<int>(nameLen));

            state.fileName = QString::fromUtf8(nameBytes);
            state.fileSize = fileSize;
            state.headerReceived = true;
            state.fileData.clear();

            ui.textEdit->append(
                QStringLiteral("收到文件头: 名称=%1, 大小=%2 字节")
                .arg(state.fileName)
                .arg(static_cast<qulonglong>(state.fileSize)));

            continue; // 继续解析后续数据帧
        }
        else {
            // 已经有 header，期待数据帧
            if (type != 1) {
                ui.textEdit->append(
                    QStringLiteral("文件数据帧 type 错误=%1，终止本次文件").arg(type));
                m_fileStates.remove(socket);
                break;
            }

            if (buffer.size() < HEADER1_SIZE) {
                // 头不完整
                break;
            }

            quint32 chunkSize = qFromBigEndian<quint32>(raw + 5);

            if (buffer.size() < HEADER1_SIZE + static_cast<int>(chunkSize)) {
                // 整块还没收完
                break;
            }

            // 丢掉数据帧头
            buffer.remove(0, HEADER1_SIZE);
            QByteArray chunkData = buffer.left(static_cast<int>(chunkSize));
            buffer.remove(0, static_cast<int>(chunkSize));

            if (chunkData.isEmpty()) {
                ui.textEdit->append(QStringLiteral("收到空数据块，忽略"));
                continue;
            }

            state.fileData.append(chunkData);

            socket->write("/ACK/" + QByteArray::number(state.fileData.size()) + "/" + state.fileName.toUtf8() + "/"+ QByteArray::number(state.fileSize)+"/"); // 可选：发送 ACK 确认收到数据块
            
            ui.textEdit->append(
                QStringLiteral("收到文件数据块: 当前累计 %1 / %2 字节")
                .arg(state.fileData.size())
                .arg(static_cast<qulonglong>(state.fileSize)));

			

            // 判断是否收完
            if (state.fileSize > 0 &&
                static_cast<quint64>(state.fileData.size()) >= state.fileSize) {
                process_data(socket,state);
                
                // 写文件
                /*QString baseDir =
                    root_path+"/files";
                QString fullPath = baseDir + "/" + state.fileName;

                QDir().mkpath(baseDir);

                QFile outFile(fullPath);
                if (outFile.open(QIODevice::WriteOnly)) {
                    outFile.write(state.fileData);
                    outFile.close();
                    ui.textEdit->append(
                        QStringLiteral("文件已保存到: %1, 实际大小: %2 字节")
                        .arg(fullPath)
                        .arg(state.fileData.size()));

                }
                else {
                    ui.textEdit->append(
                        QStringLiteral("无法写入文件: %1").arg(fullPath));
                }
                */

                // 清理本次文件状态
                m_fileStates.remove(socket);
                break; // 当前文件结束，退出循环
            }

            // 还没收完，继续 while 解析后续帧（如果有）
        }
    }
}

class FileSenderTask : public QRunnable
{
public:
    FileSenderTask(NeaseServer* owner, QTcpSocket* socket, const QString& filePath)
        : m_owner(owner), m_socket(socket), m_filePath(filePath)
    {
        setAutoDelete(true);
    }

    void run() override {
        if (!m_owner || !m_socket) return;

        QFile file(m_filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            qWarning() << "Failed to open file:" << m_filePath;
            return;
        }

        const QString fileName = QFileInfo(m_filePath).fileName();
        const QByteArray fileNameUtf8 = fileName.toUtf8();
        const quint32 nameLen = static_cast<quint32>(fileNameUtf8.size());
        const quint64 fileSize = static_cast<quint64>(file.size());
        const qint64 CHUNK_SIZE = 64 * 1024; // 64KB
        const quint32 MAGIC = 0x4E46544C;

        // 1) 发送文件头：在主线程写入 socket，并获取 pending bytes
        quint64 pending = 0;
        bool ok = QMetaObject::invokeMethod(m_owner, [this, &pending, MAGIC, nameLen, fileSize, &fileNameUtf8]() {
            // lambda 在主线程执行
            if (!m_socket || !m_socket->isOpen()) {
                pending = UINT64_MAX;
                return;
            }
            QByteArray header;
            header.resize(4 + 1 + 4 + 8); // magic + type + nameLen + fileSize
            uchar* p = reinterpret_cast<uchar*>(header.data());
            qToBigEndian(MAGIC, p);      // 0..3
            p[4] = 0;                    // type = 0 => 文件头
            qToBigEndian(nameLen, p + 5);// 5..8
            qToBigEndian(fileSize, p + 9);// 9..16 (8 字节)

            m_socket->write(header);
            m_socket->write(fileNameUtf8);
            // 不 waitForBytesWritten，这里只返回当前 pending 大小
            pending = static_cast<quint64>(m_socket->bytesToWrite());
            }, Qt::BlockingQueuedConnection);

        if (!ok || pending == UINT64_MAX) {
            qWarning() << "Socket not available when sending header:" << m_filePath;
            file.close();
            return;
        }

        // 限流阈值：当发送缓冲积累过多时，后台线程暂停读文件以免 OOM
        const quint64 MAX_PENDING = 4ull * 1024 * 1024; // 4MB

        quint64 sentBytes = 0;
        while (!file.atEnd()) {
            QByteArray chunk = file.read(CHUNK_SIZE);
            if (chunk.isEmpty()) break;

            const quint32 chunkSize = static_cast<quint32>(chunk.size());

            // 构造数据帧头（magic + type + chunkSize）
            QByteArray header;
            header.resize(4 + 1 + 4);
            uchar* p = reinterpret_cast<uchar*>(header.data());
            qToBigEndian(MAGIC, p);
            p[4] = 1; // type = 1 => 数据块
            qToBigEndian(chunkSize, p + 5);

            // 把写入操作提交到主线程（BlockingQueuedConnection），并读取写入后的 pending bytes
            pending = 0;
            bool wrote = QMetaObject::invokeMethod(m_owner, [this, &pending, header, chunk]() {
                if (!m_socket || !m_socket->isOpen()) {
                    pending = UINT64_MAX;
                    return;
                }
                m_socket->write(header);
                m_socket->write(chunk);
                // 立即读取 pending 字节数返回给后台线程
                pending = static_cast<quint64>(m_socket->bytesToWrite());
                }, Qt::BlockingQueuedConnection);

            if (!wrote || pending == UINT64_MAX) {
                qWarning() << "Socket closed during send:" << m_filePath;
                break;
            }

            sentBytes += chunkSize;

            // 简单流控：当 pending 超过阈值，后台线程轮询等待，间隔短暂休眠
            while (pending > MAX_PENDING) {
                QThread::msleep(30);
                // 查询主线程 socket pending
                bool ok2 = QMetaObject::invokeMethod(m_owner, [this, &pending]() {
                    if (!m_socket || !m_socket->isOpen()) {
                        pending = UINT64_MAX;
                        return;
                    }
                    pending = static_cast<quint64>(m_socket->bytesToWrite());
                    }, Qt::BlockingQueuedConnection);
                if (!ok2 || pending == UINT64_MAX) {
                    qWarning() << "Socket closed during throttling:" << m_filePath;
                    break;
                }
            }
        }

        file.close();
        qDebug() << "Async file send finished:" << fileName << "size=" << fileSize << "sentBytes=" << sentBytes;
    }

private:
    NeaseServer* m_owner;
    QTcpSocket* m_socket;
    QString m_filePath;
};

void NeaseServer::send_file_to_client(QTcpSocket* socket, const QString& filePath) {
    if (!socket || !socket->isOpen()) {
        qWarning() << "Socket is not valid/open";
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open file:" << filePath;
        return;
    }

    const QString fileName = QFileInfo(filePath).fileName();
    const QByteArray fileNameUtf8 = fileName.toUtf8();
    const quint32 nameLen = static_cast<quint32>(fileNameUtf8.size());
    const quint64 fileSize = static_cast<quint64>(file.size());
    const qint64 CHUNK_SIZE = 64 * 1024; // 64KB

    // ===== 1. 发送文件头帧 =====
    {
        QByteArray header;
        header.resize(4 + 1 + 4 + 8); // magic + type + nameLen + fileSize

        uchar* p = reinterpret_cast<uchar*>(header.data());
        const quint32 magic = 0x4E46544C; // "NFTL" 任意，只要前后统一
        qToBigEndian(magic, p);      // 0..3
        p[4] = 0;                    // type = 0 => 文件头
        qToBigEndian(nameLen, p + 5);// 5..8
        qToBigEndian(fileSize, p + 9);// 9..16 (8 字节)

        // 先写 header，再写文件名（UTF-8）
        socket->write(header);
        socket->write(fileNameUtf8);
        socket->flush();
        socket->waitForBytesWritten(1000);
    }

    // ===== 2. 分块发送文件数据帧 =====
    quint64 sentBytes = 0;
    while (!file.atEnd()) {
        QByteArray chunk = file.read(CHUNK_SIZE);
        if (chunk.isEmpty())
            break;

        const quint32 chunkSize = static_cast<quint32>(chunk.size());

        QByteArray header;
        header.resize(4 + 1 + 4); // magic + type + chunkSize
        uchar* p = reinterpret_cast<uchar*>(header.data());
        const quint32 magic = 0x4E46544C;
        qToBigEndian(magic, p);        // 0..3
        p[4] = 1;                      // type = 1 => 数据块
        qToBigEndian(chunkSize, p + 5);// 5..8

        socket->write(header);
        socket->write(chunk);
        socket->flush();
        socket->waitForBytesWritten(1000);

        sentBytes += chunkSize;
    }

    file.close();
    qDebug() << "File" << fileName << "sent. size =" << fileSize
        << "sentBytes =" << sentBytes;
}

void NeaseServer::process_data(QTcpSocket* socket, const FileReceiveState& state) {
    if (socket->state() != QAbstractSocket::ConnectedState) {
        // 已经不是连接状态，不再处理任何数据，防止读到乱流
        return;
    }

	QString username;

    QHostAddress peerAddr = socket->peerAddress();
    quint16 peerPort = socket->peerPort();
    QString ip = QString("%1:%2")
        .arg(peerAddr.toString())
        .arg(peerPort);
    for (int i = 0; i < model->rowCount(); i++) {
        if (model->item(i, 2)->text() == ip) {
            username = model->item(i, 0)->text();
			break;
        }
    }

    

	QString baseDir = root_path + "/"+username;
        
    QString fullPath = baseDir + "/files/" + state.fileName;

    QFile outFile(fullPath);

    if (outFile.open(QIODevice::WriteOnly)) {
        outFile.write(state.fileData);
        outFile.close();
        ui.textEdit->append(
            QStringLiteral("文件已保存到: %1, 实际大小: %2 字节")
            .arg(fullPath)
            .arg(state.fileData.size()));

        QTimer::singleShot(4000, this, [this, socket, state]() {
            if (!socket || !socket->isOpen() ||
                socket->state() != QAbstractSocket::ConnectedState) {
                return;
            }
            socket->write("/UPDONE/"+state.fileName.toUtf8()+"/");
            });
    }
    
    if(state.fileName.startsWith("%"))
    {
        const QStringList parts = state.fileName.split("%");
        if(parts.size() > 1)
        {
            if (parts[1] == "Aleain") {
				QString space = parts[1];
				QString dirName = parts[2];
				QString Name = parts[3];
				QString targetDir = root_path + "/" + username + "/files/Aleain/" + dirName;
                QFile::rename(fullPath, targetDir+ "/" + Name);
				ui.textEdit->append(QStringLiteral("已将文件移动到Aleain目录: %1").arg(targetDir + "/" + Name));
            }
        }
    }
}

void NeaseServer::writeToClient(QTcpSocket* socket, const QString& filePath) {
    if (!socket || !socket->isOpen()) {
        qWarning() << "Socket is not valid/open";
        return;
    }

    // 提交异步任务到全局线程池，立即返回，不阻塞调用者
    auto* task = new FileSenderTask(this, socket, filePath);
    QThreadPool::globalInstance()->start(task);
}

void NeaseServer::send_to_client(QTcpSocket* socket) {
    QHostAddress peerAddr = socket->peerAddress();
    quint16 peerPort = socket->peerPort();
    QString ip = QString("%1:%2")
        .arg(peerAddr.toString())
        .arg(peerPort);
    QString username;
    for (int i = 0; i < model->rowCount(); i++) {
        if (model->item(i, 2)->text() == ip) {
            username = model->item(i, 0)->text();
            break;
        }
    }
    
    QString confPath = root_path + "/" + username + "/configs/conf.as";
    if (!m_userData.contains(socket)) {
        //auto userData = m_userData[socket];
        ui.textEdit->append(QStringLiteral("未找到用户数据,开始创建"));
        m_userData[socket] = new AStruct();
    }

    auto userData = m_userData[socket];
	
    userData->loaddata(confPath.toStdString());
    {
        qint64 fileSize = 0;
		QString path = root_path + "/" + username + "/files";
        QDir dir(path);
        AList list;
        for (const QFileInfo& fileInfo : dir.entryInfoList(QDir::Files)) {
            QString fileName = fileInfo.fileName();   
			list << (AList()<<fileName.toStdString()<< fileInfo.size());
            fileSize += fileInfo.size();
        }
		userData->changeValue("main", "FileInfo", "filearray", list.toArray());
		QString maxsize = QString::fromStdString(userData->getvalue("main", "FileInfo", "dirsize")).split("/").last();
		userData->changeValue("main", "FileInfo", "dirsize", QString("%1/%2").arg(fileSize).arg(maxsize).toStdString());
    
    }



    QTimer::singleShot(800, this, [this, socket,userData]() {
        if (!socket || !socket->isOpen() ||
            socket->state() != QAbstractSocket::ConnectedState) {
            return;
        }
        socket->write("#Neaseinfo\n" + QByteArray(userData->structdata));     
    });
	
}
   /* QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(data);
        file.close();
    }*/

void NeaseServer::abortDownload(QTcpSocket* socket) {
    if (!socket) {
        return;
    }

    // 1. 停止该 socket 的文件接收状态（上传方向）
    if (m_fileStates.contains(socket)) {
        const auto& state = m_fileStates[socket];
        ui.textEdit->append(
            QStringLiteral("中止文件收发状态: %1 (已接收 %2 / %3 字节)")
            .arg(state.fileName)
            .arg(state.fileData.size())
            .arg(static_cast<qulonglong>(state.fileSize)));
        m_fileStates.remove(socket);
    }
}

void NeaseServer::registerUser(QStringList& vec) {
    bool usernamesave = false;
    QString username = vec[2];
    QString password = vec[3];
    QString usercode = vec[4];
    

    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());

    QHostAddress peerAddr = socket->peerAddress();

    quint16 peerPort = socket->peerPort();

    QString fullAddress = QString("%1:%2")
        .arg(peerAddr.toString())
        .arg(peerPort);

    for (int i = 0; i < model->rowCount(); i++) {
         if(model->item(i,0)->text() == username){
            socket->write("/ES/");
            usernamesave = true;
            return;
         }
    }

    if (!(ui.textEdit_2->toPlainText() == usercode)) {
        socket->write("/ESCODE/");
        return;
    }

    if (!usernamesave) {
        model->appendRow(new QStandardItem(username));
		model->setItem(model->rowCount() - 1, 1, new QStandardItem(password));
		model->setItem(model->rowCount() - 1, 2, new QStandardItem(fullAddress));
		model->setItem(model->rowCount() - 1, 3, new QStandardItem("offline"));
		socket->write("/ESS/");
        AList list;
		list << username.toStdString() << password.toStdString();
		main->addkey("Main", "Users", username.toStdString(), list.toArray());
        QDir dir;
		dir.mkdir(root_path + "/" + username);
		dir.mkdir(root_path + "/" + username+"/files");
		dir.mkdir(root_path + "/" + username+"/configs");

        m_userData[socket] = new AStruct();
        auto userData = m_userData[socket];
        userData->CreateStruct(root_path.toStdString() + "/" + username.toStdString() + "/configs/conf.as", "main", "FileInfo", "dirsize", "0/10737418240");
		userData->singlesaved("welcome to use AleacNease", root_path.toStdString() + "/" + username.toStdString() + "/files/readme.txt");
        userData->loaddata(root_path.toStdString() + "/" + username.toStdString() + "/configs/conf.as");
		userData->AppendHeader("main", "Configs", "MaxSpeed", "10");
		userData->addkey("main", "Configs", "User", username.toStdString());
		userData->addkey("main", "Configs", "UserArray", "Normal");
		userData->addkey("main", "Configs", "TaskMAX", "3");
		userData->addkey("main", "FileInfo", "filearray", "value");
        userData->clear();
    }
	//ui.textEdit->append(QString("用户名:%1 用户密码:%2 用户代码:%3").arg(username).arg(password).arg(usercode));
    
}

void NeaseServer::loggin(QStringList& vec) {
    QString username = vec[2];
    QString password = vec[3];
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    QHostAddress peerAddr = socket->peerAddress();
    quint16 peerPort = socket->peerPort();
    QString fullAddress = QString("%1:%2")
        .arg(peerAddr.toString())
        .arg(peerPort);
    for (int i = 0; i < model->rowCount(); i++) {
         if(model->item(i,0)->text() == username && model->item(i,1)->text() == password){
			 if (model->item(i, 3)->text() == "online") {
                socket->write("/EX/");
                return;
            }
            model->setItem(i, 2, new QStandardItem(fullAddress));
            model->setItem(i, 3, new QStandardItem("online"));
            socket->write("/ESL/");
			send_to_client(socket);
            return;
         }
    }

    socket->write("/ESEL/");
}

void NeaseServer::onClientDisconnected() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    abortDownload(socket);

    m_clients.removeAll(socket);

	ui.textEdit->append(QString("客户端断开连接: %1").arg(socket->peerAddress().toString()));

    QHostAddress peerAddr = socket->peerAddress();

    quint16 peerPort = socket->peerPort();

    QString fullAddress = QString("%1:%2")
        .arg(peerAddr.toString())
        .arg(peerPort);

	for (int i = 0; i < model->rowCount(); i++) {
        if(model->item(i,2)->text() == fullAddress){
			model->setItem(i, 3, new QStandardItem("offline"));
			model->setItem(i, 2, new QStandardItem("NULL"));
            break;
        }
    }

    m_fileStates.remove(socket);

    socket->deleteLater();
}

void NeaseServer::process_Aleain(QTcpSocket* socket, const QString& Path) {
    {
        AList list;
        QDir dir(Path);
        for (const QFileInfo& fileInfo : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
			list << fileInfo.fileName().toStdString();
        }
        socket->write("#key=" + QString::fromStdString(list.toArray()).toUtf8());
    }
}

NeaseServer::~NeaseServer()
{}

