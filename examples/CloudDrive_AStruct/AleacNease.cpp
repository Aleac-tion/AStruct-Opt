#include "AleacNease.h"
#include <qstringfwd.h>
#include <QRegularExpression>
#include <QMessageBox>
#include <QRegularExpressionValidator>
#include <QFile>
#include <QFileDialog>
#include <QDir>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QThreadPool>
#include <QTreeView>
#include <QTreeWidget>
#include <QRunnable>
#include <QInputDialog>
#include <QMenu>
#include <QFileInfo>
#include <QtEndian>
#include <QMetaObject>
#include <QDebug>
#include <QTimer>
#include <QDragEnterEvent>
#include <QTWidgets>

QIcon getSystemFileIcon(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    QFileIconProvider iconProvider;
    return iconProvider.icon(fileInfo);
}

AleacNease::AleacNease(QWidget* parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    connect(m_socket, &QTcpSocket::connected,
        this, &AleacNease::onConnected);
    connect(m_socket, &QTcpSocket::disconnected,
        this, &AleacNease::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead,
        this, &AleacNease::onReadyRead);
    connect(m_socket, &QTcpSocket::errorOccurred,
        this, &AleacNease::onError);

    main->loaddata(AStruct::static_getDir() + "/main.as");

    download_path = QString::fromLocal8Bit( main->getvalue("Main", "paths", "download"));	

    ui.tabWidget->setStyleSheet("QTabWidget::pane { border: 0; } QTabBar::tab { height: 0; }");

    QRegularExpression regex("[a-zA-Z0-9]+");
    QRegularExpressionValidator* validator = new QRegularExpressionValidator(regex, this);

    ui.lineEdit->setValidator(validator);
    ui.lineEdit_3->setValidator(validator);
    ui.lineEdit_4->setValidator(validator);
    ui.lineEdit_5->setValidator(validator);

    ui.progressBar->setStyleSheet("QProgressBar {"
        "   border: 1px solid #b3b3b3;"
        "   background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f0f0f0, stop:1 #e0e0e0);"
        "   height: 20px;"
        "   minimum-height: 20px;"
        "   text-align: center;"
        "}"
        "QProgressBar::chunk {"
        "   background-color: #0078d4;"
        "   border-radius: 5px;"
        "   margin: 1px;"
        "}");

    ui.DIR_bar->setStyleSheet("QProgressBar {"
        "   border: 1px solid #b3b3b3;"
        "   background-color: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #f0f0f0, stop:1 #e0e0e0);"
        "   height: 20px;"
        "   minimum-height: 20px;"
        "   text-align: center;"
        "}"
        "QProgressBar::chunk {"
        "   background-color: #0078d4;"
        "   border-radius: 5px;"
        "   margin: 1px;"
        "}");

    {
        ui.tree->setFrameShape(QFrame::NoFrame);
        ui.tree->setStyleSheet(R"(
    QTreeWidget {
        background: transparent;
        border: 8px solid #add8e6;   /* ← 整个控件的浅蓝色边框 */
        outline: 0;
        border-radius: 12px;  
    }
    QTreeWidget::item {
        height: 32px;
        padding-left: 8px;
        border: 1px solid #add8e6; /* 浅蓝色边框 */
        border-radius: 4px; /* 可选：圆角边框 */
    }
    QTreeWidget::item:selected {
        background: #e6f0ff;
        color: black;
        border-color: #6495ed; /* 选中时边框颜色加深 */
    }
)");
        ui.tree->setContextMenuPolicy(Qt::CustomContextMenu);
        ui.tree->setIconSize(QSize(24, 24));
        connect(ui.tree, &QTreeWidget::customContextMenuRequested,
            this, &AleacNease::onTreeContextMenuRequested);
        ui.tree->setHeaderHidden(true);
        //QTreeWidgetItem* folderItem = new QTreeWidgetItem(ui.tree);
        folderItem = new QTreeWidgetItem(ui.tree);
        folderItem->setText(0, "根目录");
        folderItem->setData(0, Qt::UserRole, true);     

        if (!m_progressLayout) {
            m_progressLayout = ui.verticalLayout; 
            m_progressLayout->setSpacing(8);
            m_progressLayout->addStretch();
        }

        ui.lineEdit_6->setText(main->getvalue("Main", "config", "ip").c_str());
        ui.lineEdit_2->setText(main->getvalue("Main", "config", "port").c_str());

        ui.label_15->setAlignment(Qt::AlignCenter);
        ui.label_15->setFrameStyle(QFrame::Box);

        ui.label_15->installEventFilter(this);
        ui.label_15->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 10px; }");
        
        if (Auther->listen(QHostAddress("127.0.0.1"), 11100)) {
            connect(Auther, &QTcpServer::newConnection, this, &AleacNease::onNewConnection);
        }
        else {
			QMessageBox::information(this, "错误", "无法启动授权端口，授权可能会失败！");
        }
    }

    connectToServer();
}

void AleacNease::on_pushButton_clicked() {
    main->changeValue("Main", "config", "ip", ui.lineEdit_6->text().toStdString());
    main->changeValue("Main", "config", "port", ui.lineEdit_2->text().toStdString());
	QMessageBox::information(this, "提示", "配置已保存,请重启应用以生效");
    exit(0);
}

class FileSendTask : public QRunnable
{
public:
    FileSendTask(const QString& filePath, QObject* receiver)
        : m_filePath(filePath), m_receiver(receiver)
    {
    }

    void run() override
    {
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

        quint64 sentBytes = 0;

        // 构造文件头帧：magic + type + nameLen + fileSize
        {
            QByteArray header;
            header.resize(4 + 1 + 4 + 8); // magic + type + nameLen + fileSize

            uchar* p = reinterpret_cast<uchar*>(header.data());
            const quint32 magic = 0x4E46544C; // "NFTL"
            qToBigEndian(magic, p);      // 0..3
            p[4] = 0;                    // type = 0 => 文件头
            qToBigEndian(nameLen, p + 5);// 5..8
            qToBigEndian(fileSize, p + 9);// 9..16 (8 字节)

            // 把 header 和文件名调度回主线程写入 socket
            QMetaObject::invokeMethod(m_receiver, "writeToSocket", Qt::QueuedConnection, Q_ARG(QByteArray, header));
            QMetaObject::invokeMethod(m_receiver, "writeToSocket", Qt::QueuedConnection, Q_ARG(QByteArray, fileNameUtf8));

            // 初始进度：0/total
            QMetaObject::invokeMethod(m_receiver, "onSendProgress", Qt::QueuedConnection, Q_ARG(quint64, sentBytes), Q_ARG(quint64, fileSize));
        }

        // 分块读取并构造数据帧，然后调度主线程写入，并回报进度
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

            QMetaObject::invokeMethod(m_receiver, "writeToSocket", Qt::QueuedConnection, Q_ARG(QByteArray, header));
            QMetaObject::invokeMethod(m_receiver, "writeToSocket", Qt::QueuedConnection, Q_ARG(QByteArray, chunk));

            sentBytes += chunkSize;

            // 报告进度（queued 到主线程）
            QMetaObject::invokeMethod(m_receiver, "onSendProgress", Qt::QueuedConnection, Q_ARG(quint64, sentBytes), Q_ARG(quint64, fileSize));
        
        }

        file.close();
        qDebug() << "FileSendTask finished for" << fileName << "size =" << fileSize;
    }

private:
    QString m_filePath;
    QObject* m_receiver = nullptr; // 应为 AleacNease 的实例，用于 invokeMethod 写 socket
};

void AleacNease::onTreeContextMenuRequested(const QPoint& pos) {
    QPoint globalPos = ui.tree->viewport()->mapToGlobal(pos);

    QTreeWidgetItem* item = ui.tree->itemAt(pos);

    QMenu menu;

    if (item) {
        // 点击在某个 item 上
        QString itemName = item->text(0);
        bool isDirectory = item->data(0, Qt::UserRole).toBool(); // 使用 UserRole 判断是否为目录
		bool isRoot =false; // 根目录特殊处理
        {
            if (isDirectory) {
                if (itemName == "根目录") {
                    isRoot = true;

                }
            }
           
        }

        if (isDirectory) {
            if (isRoot)return;
            // 目录：只允许删除（或也可加“进入”）       
            menu.addAction("删除目录", [this, item]() {
                handleDeleteItem(item);
                });
        }
        else {
            // 文件：允许下载 + 删除
            menu.addAction("下载", [this, item]() {
                handleDownloadItem(item);
                });
            menu.addAction("删除", [this, item]() {
                handleDeleteItem(item);
                });
        }
    }
    else {
        // 点击在空白区域 → 新建目录
        menu.addAction("新建目录", [this]() {
            handleCreateNewDirectory();
            });
    }

    // 弹出菜单
    if (!menu.isEmpty()) {
        menu.exec(globalPos);
    }


}

void AleacNease::onTaskAdded(const QString& fileName) {
    if (m_taskWidgets.contains(fileName)) {
        return; // 防止重复添加
    }

    QLabel* label = new QLabel(fileName, this);
    label->setStyleSheet("font-weight: bold;");

    QProgressBar* progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);

    m_progressLayout->insertWidget(0, label);
    m_progressLayout->insertWidget(1, progressBar);

    m_taskWidgets[fileName] = qMakePair(label, progressBar);
}

void AleacNease::onTaskProgressUpdated(const QString& fileName, int percent) {
    if (!m_taskWidgets.contains(fileName)) {
        return;
    }

    QProgressBar* bar = m_taskWidgets[fileName].second;
    bar->setValue(percent);
}

void AleacNease::onTaskRemoved(const QString& fileName) {
    if (!m_taskWidgets.contains(fileName)) {
        return;
    }

    auto [label, progressBar] = m_taskWidgets[fileName];

    m_progressLayout->removeWidget(label);
    m_progressLayout->removeWidget(progressBar);
    delete label;
    delete progressBar;

    m_taskWidgets.remove(fileName);
}

void AleacNease::zipDirectory(const QString& dirPath, const QString& outputZipPath) {
    QString absDir = QDir::toNativeSeparators(QDir(dirPath).absolutePath());

    QString zipFile = QFileInfo(outputZipPath).absoluteFilePath();

    QStringList args;
    args << "-Command"
        << QString("Compress-Archive -Path \"%1\" -DestinationPath \"%2\" -Force")
        .arg(absDir)
        .arg(zipFile);
    
    int result = QProcess::execute("powershell.exe", args);

    if (result == 0) {
        // ✅ 压缩成功，到这里就说明 ZIP 已经完全生成
		QMessageBox::information(this, "压缩完成", QString("即将发送[%1]到服务器").arg(zipFile));       

        if (TaskList.empty()) {
            FileSendTask* task = new FileSendTask(zipFile, this);
            task->setAutoDelete(true);
            QThreadPool::globalInstance()->start(task);
        }

        if (TaskList.contains("UP:" + QFileInfo(zipFile).fileName())) {
            QMessageBox::information(this, "任务已存在", "不能重复添加相同的任务");
            return;
        }

        if (TaskList.contains("DOWN:")) {
            QMessageBox::information(this, "存在下载任务", "当前有下载任务在进行，建议等待下载完成后再添加上传任务");
            return;
        }

        if (TaskList.size() >= taskmax) {
            QMessageBox::information(this, "任务已满", QString("当前任务数已达上限(%1)，请等待现有任务完成后再添加").arg(taskmax));
            return;
        }

        TaskList.push_back("UP:" + zipFile);
        onTaskAdded("上传:" + QFileInfo(zipFile).fileName());

		//QFile::remove(zipFile); // 删除临时 ZIP 文件
    }
    else {
		QMessageBox::information(this, "压缩失败", "目录压缩失败，请检查目录路径和权限");
        qWarning() << "ZIP failed with exit code:" << result;
    }

    
}



void AleacNease::handleDownloadItem(QTreeWidgetItem* item) {
    QString fileName = item->text(1);
    QString Username = QString::fromStdString(post_as->getvalue("main", "Configs", "User"));
	QString url = QString("#cmd/DL/%1/%2").arg(Username, fileName);
    QString filePath = QFileDialog::getExistingDirectory(this, "选择文件夹",download_path);
	if (filePath.isEmpty()) return;
    download_path = filePath;
    main->changeValue("Main", "paths", "download", download_path.toLocal8Bit().toStdString());
	onTaskAdded("下载:"+fileName);
    if (TaskList.isEmpty()) {
        sendMessage(url);
    }

    if (TaskList.contains("DOWN:" + Username + "/" + fileName)) {
        QMessageBox::information(this, "任务已存在", "不能重复添加相同的任务");
        return;
    }

    if (TaskList.contains("UP:")) {
		QMessageBox::information(this, "正在上传", "请等待当前上传任务完成后再添加下载任务");
        return;
    }

    if (TaskList.size() >= taskmax) {
        QMessageBox::information(this, "任务已满", QString("当前任务数已达上限(%1)，请等待现有任务完成后再添加").arg(taskmax));
        return;
    }

    TaskList.push_back("DOWN:" + Username+"/"+fileName);
	
}

void AleacNease::handleDeleteItem(QTreeWidgetItem* item) {
    QString fileName = item->text(1);
    bool isDir = (item->childCount() > 0);

    // 弹出确认对话框（可选）
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "确认删除",
        QString("确定要删除 %1「%2」吗？").arg(isDir ? "目录" : "文件").arg(fileName),
        QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes) {
        // 从树中移除
        //delete item; 
        QString Username = QString::fromStdString(post_as->getvalue("main", "Configs", "User"));
        QString url = QString("#cmd/RL/%1/%2").arg(Username, fileName);
		sendMessage(url);
    }
    
}

void AleacNease::handleCreateNewDirectory() {
    bool ok;
    QString dirName = QInputDialog::getText(this, "新建目录", "请输入目录名称：", QLineEdit::Normal, "", &ok);
    if (ok && !dirName.isEmpty()) {
        QTreeWidgetItem* newDir = new QTreeWidgetItem(ui.tree);
        newDir->setText(0, dirName);
    }
}

void AleacNease::sendMessage(const QString& message) {
    if (message.isEmpty()) return;
    m_socket->write(message.toUtf8() + "\n");
}

void AleacNease::onDisconnected()
{
    m_socket->disconnectFromHost();
}

void AleacNease::onError(QAbstractSocket::SocketError socketError)
{
    QString errorMsg;
    switch (socketError) {
    case QAbstractSocket::ConnectionRefusedError:
        errorMsg = "连接被拒绝";
        break;
    case QAbstractSocket::HostNotFoundError:
        errorMsg = "服务器未找到";
        break;
    case QAbstractSocket::NetworkError:
        errorMsg = "网络错误";
        break;
    default:
        errorMsg = m_socket->errorString();
    }
    QMessageBox::critical(this, "连接错误", errorMsg);
}

void AleacNease::sendBinaryData(const QByteArray& data) {
    if (data.isEmpty()) return;
    quint32 dataSize = static_cast<quint32>(data.size());
    QByteArray sizePrefix;
    sizePrefix.resize(sizeof(dataSize));
    qToBigEndian(dataSize, reinterpret_cast<uchar*>(sizePrefix.data()));

    // 2. 发送：长度头 + 原始数据
    m_socket->write(sizePrefix);
    m_socket->write(data); // 直接发送原始字节，不加任何修饰
}

bool AleacNease::sendFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "文件错误", "无法打开文件");
        return false;
    }
    QByteArray fileData = file.readAll();
    QByteArray filename = filePath.toUtf8();
    if (fileData.size() > 64 * 1024) {
        sendLargeFile(m_socket, filePath);
        return true;
    }

    sendBinaryData(fileData + "--named:" + filename.replace("\\", "/").split('/').last());
    return true;
}

void AleacNease::onReadyRead()
{
    QByteArray data = m_socket->readAll();

	if (data.isEmpty()) return;


    QString message = QString::fromUtf8(data).trimmed();
    if (message == "/ES/") {
        QMessageBox::information(this, "注册失败", "目标账户已存在");
        return;
    }
    else if (message == "/ESS/") {
        QMessageBox::information(this, "注册成功", "账户注册成功，请登录");
        return;

    }
    else if (message == "/ESL/") {
        ui.tabWidget->setCurrentIndex(1);
        return;
    }
    else if (message == "/ESEL/") {
        QMessageBox::information(this, "登录失败", "用户名或密码错误");
        return;
    }
    else if (message == "/EX/") {
        QMessageBox::information(this, "登录失败", "目标账户已登录，请联系作者或立即冻结账户");
        return;
    }
    else if (message.startsWith("/ACK/")) {
		auto vec = message.split('/');
        qint64 current = vec[2].toLongLong();
        qint64 total = vec[4].toLongLong();

        int percent = 0;

        if (total > 0) {
            percent = static_cast<int>((current * 100) / total);
            if (percent > 100) {
                percent = 100;
            }
        }

		onTaskProgressUpdated("上传:"+vec[3], percent);

        //ui.progressBar->setValue(percent);
        return;
    }
    else if (message.startsWith("/UPDONE/")) {
        m_socket->write("#cmd/UPL/"); 
        auto vec = message.split('/');      
        if (vec.size() < 3) return;

        QString doneName = vec[2]; // 服务端返回的完成文件名
        // 1) 在 TaskList 中找到并移除与 doneName 对应的条目（TaskList 存储格式为 "UP:<fullPath>"）
        int removedIndex = -1;
        for (int i = 0; i < TaskList.size(); ++i) {
            const QString& entry = TaskList[i];
            if (entry.startsWith("UP:")) {
                QString path = entry.mid(3);
                if (QFileInfo(path).fileName() == doneName) {
                    TaskList.removeAt(i);
                    removedIndex = i;
                    break;
                }
            }
        }
		QFile::remove(QString::fromLocal8Bit(AStruct::static_getDir())+"/TempSource/"+doneName); // 删除临时 ZIP 文件

        onTaskRemoved("上传:" + doneName);

        int nextIdx = -1;
        for (int i = 0; i < TaskList.size(); ++i) {
            if (TaskList[i].startsWith("UP:")) {
                nextIdx = i;
                break;
            }
        }

        if (nextIdx != -1) {
            QString nextPath = TaskList[nextIdx].mid(3);
            QString nextFileName = QFileInfo(nextPath).fileName();

            // 将下一个任务加入 UI（确保进度条/任务项存在），并稍作提示（不使用阻塞对话框）
            onTaskProgressUpdated("上传:" + nextFileName, 0);
            //ui.textEdit->append(QString("上传完成: %1，准备开始下一个文件: %2").arg(doneName, nextFileName));
            //QMessageBox::information(nullptr, "title", QString("上传完成: %1，准备开始下一个文件: %2").arg(doneName, nextFileName));
            // 稍微延迟启动，给 UI 更新时间
            QTimer::singleShot(800, this, [this, nextPath]() {
                FileSendTask* task = new FileSendTask(nextPath, this);
                task->setAutoDelete(true);
                QThreadPool::globalInstance()->start(task);
                });
        }
        else {
            // 没有更多任务，重置进度并通知服务器（保留原行为）
            ui.textEdit->append(QString("上传完成: %1，当前无更多上传任务").arg(doneName));
            ui.progressBar->setValue(0);
            
        }

        return;

        //ui.progressBar->setValue(0);
        //m_socket->write("#cmd/UPL/");
    }		                                                                                    
	else if (message.startsWith("#Neaseinfo")) {
        post_as->clear();
        post_as->structdata = message.toStdString();
        process_client_message();
        return;
    }
    else if (message == "/ESCODE/") {
        QMessageBox::information(this, "错误", "注册码不存在，请联系作者");
		return;
    }
    else {
    }
    
    FileReceiveState& state = m_fileStates[m_socket];
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
            m_fileStates.remove(m_socket);
            break;
        }

        uchar type = raw[4];

        if (!state.headerReceived) {
            // 期待的是文件头帧
            if (type != 0) {
                ui.textEdit->append(
                    QStringLiteral("未收到文件头却收到了 type=%1，丢弃").arg(type));
                m_fileStates.remove(m_socket);
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
                m_fileStates.remove(m_socket);
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

            //m_socket->write("/ACK/" + QByteArray::number(state.fileData.size()) + "/" + state.fileName.toUtf8() + "/" + QByteArray::number(state.fileSize) + "/"); // 可选：发送 ACK 确认收到数据块
            
            onTaskProgressUpdated("下载:"+state.fileName, static_cast<int>((state.fileData.size() * 100) / state.fileSize));



            ui.textEdit->append(
                QStringLiteral("收到文件数据块: 当前累计 %1 / %2 字节")
                .arg(state.fileData.size())
                .arg(static_cast<qulonglong>(state.fileSize)));



            // 判断是否收完
            if (state.fileSize > 0 &&
                static_cast<quint64>(state.fileData.size()) >= state.fileSize) {
                //process_data(socket, state)
                process_download_data(m_socket, state);
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
                m_fileStates.remove(m_socket);
                break; // 当前文件结束，退出循环
            }

            // 还没收完，继续 while 解析后续帧（如果有）
        }
    }
}

void AleacNease::process_download_data(QTcpSocket* socket, const FileReceiveState& state) {
    QString baseDir = download_path + "/" + state.fileName;
    QFile outFile(baseDir);
    if (outFile.open(QIODevice::WriteOnly)) {
        outFile.write(state.fileData);
        outFile.close();
        ui.textEdit_2->append(
            QStringLiteral("文件已保存到: %1, 实际大小: %2 字节")
            .arg(baseDir)
            .arg(state.fileData.size()));
    }
    else {
        ui.textEdit->append(
            QStringLiteral("无法写入文件: %1").arg(baseDir));

	}

	QString doneName = state.fileName; // 服务端返回的完成文件名

    int removedIndex = -1;
    for (int i = 0; i < TaskList.size(); ++i) {
        const QString& entry = TaskList[i];
        if (entry.startsWith("DOWN:")) {
            QString path = entry.mid(5).split("/").last();
            if (QFileInfo(path).fileName() == doneName) {
                TaskList.removeAt(i);
                removedIndex = i;
                break;
            }
        }
    }

	onTaskRemoved("下载:"+ doneName);

    int nextIdx = -1;
    for (int i = 0; i < TaskList.size(); ++i) {
        if (TaskList[i].startsWith("DOWN:")) {
            nextIdx = i;
            break;
        }
    }


    if (nextIdx != -1) {
        QString nextPath = TaskList[nextIdx].mid(5);
		QString nextFileName = nextPath.split("/").last();
		QString UserName = nextPath.split("/").first();

        // 将下一个任务加入 UI（确保进度条/任务项存在），并稍作提示（不使用阻塞对话框）
        onTaskProgressUpdated("下载:" + nextFileName, 0);
        //ui.textEdit->append(QString("上传完成: %1，准备开始下一个文件: %2").arg(doneName, nextFileName));
        //QMessageBox::information(nullptr, "title", QString("上传完成: %1，准备开始下一个文件: %2").arg(doneName, nextFileName));
        // 稍微延迟启动，给 UI 更新时间
        QTimer::singleShot(800, this, [this, UserName, nextFileName]() {
			sendMessage(QString("#cmd/DL/%1/%2").arg(UserName, nextFileName));
            });        
    }
    else {
        // 没有更多任务，重置进度并通知服务器（保留原行为）
        ui.textEdit->append(QString("上传完成: %1，当前无更多上传任务").arg(doneName));
        ui.progressBar->setValue(0);

    }

    return;






   
		/*auto vec = TaskList[1].split("DOWN:");
		QMessageBox::information(this, "下载完成", QString("1=%1 | 2=%2").arg(state.fileName, vec[1].split("/").last()).arg(vec[1].split("/").last()));
		QString nextFileName = vec[1].split("/").first();
		QString nextUserName = vec[1].split("/").last();
        onTaskProgressUpdated("上传:" + nextFileName, 0);

		//QMessageBox::information(this, "下载完成", QString("下载完成: %1，准备开始下一个文件: %2").arg(state.fileName, nextFileName));

        QTimer::singleShot(800, this, [this, nextFileName, nextUserName]() {

            });*/
}

void AleacNease::on_upfile_2_clicked() {
	m_socket->write("#cmd/UPL/"); // 发送上传列表请求
}

QString AleacNease::return_size(qint64 size) {
    if (size < 1024) {
        return QString("%1 B").arg(size);
    } else if (size < 1024 * 1024) {
        return QString("%1 KB").arg(size / 1024.0, 0, 'f', 2);
    } else if (size < 1024 * 1024 * 1024) {
        return QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 2);
    } else {
        return QString("%1 GB").arg(size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
    }
}

void AleacNease::process_client_message() {
	taskmax = QString::fromStdString(post_as->getvalue("main", "Configs", "TaskMAX")).toInt();
    //存储空间处理
    {
        QString dirsize = QString::fromStdString(post_as->getvalue("main", "FileInfo", "dirsize"));

        double CurrentDirSize = dirsize.split('/').first().toDouble();
        double MaxDirSize = dirsize.split('/').last().toDouble();

        int percent = 0;
        if (MaxDirSize > 0) {
            percent = static_cast<int>((CurrentDirSize * 100) / MaxDirSize);
            if (percent > 100) {
                percent = 100;
            }
        }
        ui.DIR_bar->setRange(0, 100);
        ui.DIR_bar->setValue(percent);

        ui.DIR_bar->setFormat(
            QString("%1 MiB/%2 MiB (总%3 GB)(%4GB 可用)")
            .arg(CurrentDirSize / 1024.0 )       // 整数 MiB
            .arg(MaxDirSize / 1024.0 )
			.arg(MaxDirSize / 1024.0 / 1024.0 / 1024.0, 0, 'f', 2) // 小数 GB
            .arg((MaxDirSize - CurrentDirSize) / 1024.0 / 1024.0 / 1024.0, 0, 'f', 2) // 可用 GB
        );
        dirmax = (MaxDirSize - CurrentDirSize) / 1024.0;
    } 

	//用户信息处理
    {
        ui.label_10->setText(QString("你好: %1").arg(QString::fromStdString(post_as->getvalue("main", "Configs", "User"))));
        ui.label_11->setText(QString("当前限速 %1Mbps").arg(QString::fromStdString(post_as->getvalue("main", "Configs", "MaxSpeed"))));
        ui.label_12->setText(QString("用户分组：%1").arg(QString::fromStdString(post_as->getvalue("main", "Configs", "UserArray"))));
        ui.label_13->setText(QString("任务线程上限：%1").arg(taskmax));
    }

    //文件信息处理
    {
                

        if (ui.tree->topLevelItemCount() > 0) {
            folderItem = ui.tree->topLevelItem(0);
            qDeleteAll(folderItem->takeChildren());  // 一键删除所有子项
        }
        else {
            folderItem = new QTreeWidgetItem(ui.tree);
            folderItem->setText(0, "根目录");
            folderItem->setData(0, Qt::UserRole, true);
        }

        AList list;
        list = AList::autoParse(post_as->getvalue("main", "FileInfo", "filearray"));
        for (size_t i = 0; i < list.size(); ++i) {
            QString fileName = QString::fromStdString(list[i]);
            QTreeWidgetItem* fileItem = new QTreeWidgetItem(folderItem);
            qint64 size =  QString::fromStdString(list[i].Go()[1]).toLongLong();
            
            fileItem->setText(0, QString::fromStdString(list[i].Go()[0]) + " 大小:" + return_size(size));//QString::number(size/1024.0) + "Mib");
            fileItem->setText(1, QString::fromStdString(list[i].Go()[0]));
            fileItem->setData(0, Qt::UserRole, false); // 标记为文件
		}
    }

	ui.textEdit->append(QString::fromStdString(post_as->structdata));
}

bool AleacNease::eventFilter(QObject* watched, QEvent* event) {
    // 仅对 ui.label_15 处理拖拽事件：只允许拖入且只能拖入一个目录，并将目录路径输出
    if (watched == ui.label_15) {
        switch (event->type()) {
        case QEvent::DragEnter: {
            QDragEnterEvent* dragEvent = static_cast<QDragEnterEvent*>(event);
            if (dragEvent->mimeData()->hasUrls()) {
                const QList<QUrl> urls = dragEvent->mimeData()->urls();
                // 只接受单个本地目录
                int localDirCount = 0;
                for (const QUrl& url : urls) {
                    if (!url.isLocalFile()) continue;
                    QFileInfo fi(url.toLocalFile());
                    if (fi.isDir()) ++localDirCount;
                }

                if (urls.size() == 1 && localDirCount == 1) {
                    dragEvent->acceptProposedAction();
                    ui.label_15->setStyleSheet("background-color: #e0e0ff; border: 2px dashed blue;");
                }
                else {
                    dragEvent->ignore();
                }
            }
            return true;
        }
        case QEvent::DragLeave: {
            ui.label_15->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 10px; }");
            return true;
        }
        case QEvent::Drop: {
            QDropEvent* dropEvent = static_cast<QDropEvent*>(event);
            if (!dropEvent->mimeData()->hasUrls()) {
                ui.label_15->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 10px; }");
                return true;
            }

            const QList<QUrl> urls = dropEvent->mimeData()->urls();
            // 收集本地目录（忽略文件）
            QStringList dirPaths;
            for (const QUrl& url : urls) {
                if (!url.isLocalFile()) continue;
                QString local = url.toLocalFile();
                QFileInfo fi(local);
                if (fi.isDir()) dirPaths.append(QDir::cleanPath(local));
            }

            if (dirPaths.size() != 1) {
                // 不是恰好一个目录 → 提示并忽略
                QMessageBox::warning(this, "拖拽错误", "请仅拖入一个目录（不接受文件或多个目录）");
                ui.label_15->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 10px; }");
                dropEvent->ignore();
                return true;
            }

            // 成功：取得目录路径，保存并输出A
            QString dir = dirPaths.first();
            download_path = dir; // 将拖入目录设置为下载目录（根据需要可更改）
            ui.label_15->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 10px; }");
            bool ok;
            QString dirName = QInputDialog::getText(this, "新建目录", "请输入目录名称：", QLineEdit::Normal, 
                dir.replace('\\',"/").split("/").last(), &ok);
            if (!ok || dirName.isEmpty())return true;
            ZIPPath = QString::fromLocal8Bit(AStruct::static_getDir()) + "/TempSource/" + dirName + ".zip";
            zipDirectory(dir, ZIPPath);
            dropEvent->acceptProposedAction();
            return true;
        }
        default:
            break;
        }
    }

    // 其他情况交给基类处理
    return QWidget::eventFilter(watched, event);
}

void AleacNease::on_loggin_clicked()
{
    QString username = ui.lineEdit->text();
    QString password = ui.lineEdit_3->text();
    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请填写用户名和密码");
        return;
    }
    QString message = QString("#cmd/LG/%1/%2").arg(username, password);
    sendMessage(message);
}

void AleacNease::onConnected()
{

}

void AleacNease::connectToServer() 
{
    //m_socket->connectToHost("aleac.top", 47483);
    //m_socket->connectToHost("127.0.0.1", 1234);
    //m_socket->connectToHost("240e:3a3:71a3:96f0:ffca:507c:abea:fd8d", 1234);
    m_socket->connectToHost(ui.lineEdit_6->text(), ui.lineEdit_2->text().toUInt());
}

void AleacNease::on_register_2_clicked()
{
    QString username = ui.lineEdit->text();
    QString password = ui.lineEdit_3->text();
    QString confirmPassword = ui.lineEdit_4->text();
    QString code = ui.lineEdit_5->text();
    if (username.isEmpty() || password.isEmpty() || confirmPassword.isEmpty() || code.isEmpty()) {
        QMessageBox::warning(this, "输入错误", "请填写所有字段");
        return;
    }
    if (password != confirmPassword) {
        QMessageBox::warning(this, "输入错误", "两次输入的密码不一致");
        return;
    }

    QString message = QString("#cmd/RE/%1/%2/%3").arg(username, password, code);

    sendMessage(message);
}

// -------- 工作任务：在后台线程做文件读取和帧构造，只把待写的数据调度回主线程写入 socket --------

// --------------------------------------------------------------------------------------------

void AleacNease::on_upfile_clicked()
{
    QString filePath = QFileDialog::getOpenFileName(this, "选择文件");
    if (filePath.isEmpty()) return;
	qint64 fileSize = QFileInfo(filePath).size()/1024;
	if (fileSize > dirmax) {
        QMessageBox::warning(this, "文件过大", QString("文件大小:%1 MIB\n当前可用空间:%2 MIB").arg(fileSize).arg(dirmax));
        return;
    }
    //QMessageBox::warning(this, "文件过大", QString("文件大小:%1/可用空间:%2").arg(fileSize).arg(dirmax));
    // 使用线程池提交任务（后台读取并构造帧，写操作会通过 writeToSocket 在主线程执行）
    
    //FileSendTask* task = new FileSendTask(filePath, this);
    //task->setAutoDelete(true);
    //QThreadPool::globalInstance()->start(task);
    if (TaskList.empty()) {
        FileSendTask* task = new FileSendTask(filePath, this);
        task->setAutoDelete(true);
        QThreadPool::globalInstance()->start(task);
    }

    if (TaskList.contains("UP:" + QFileInfo(filePath).fileName())) {
		QMessageBox::information(this, "任务已存在", "不能重复添加相同的任务");
        return;
    }

    if (TaskList.contains("DOWN:")) {
        QMessageBox::information(this, "存在下载任务", "当前有下载任务在进行，建议等待下载完成后再添加上传任务");
		return;
    }

	if (TaskList.size() >= taskmax) {
        QMessageBox::information(this, "任务已满", QString("当前任务数已达上限(%1)，请等待现有任务完成后再添加").arg(taskmax));
        return;
    }

    TaskList.push_back("UP:" + filePath);
    onTaskAdded("上传:" + QFileInfo(filePath).fileName());
    ui.progressBar->setValue(0);
}

// 主线程中的槽：负责把数据写入 socket（被后台任务通过 Qt::QueuedConnection 调用）
void AleacNease::writeToSocket(const QByteArray& data)
{
    if (!m_socket || !m_socket->isOpen()) {
        qWarning() << "Socket is not valid/open when trying to write";
        return;
    }
    m_socket->write(data);
}
// 新增：接收进度并更新 UI（格式： 当前size/总size）
void AleacNease::onSendProgress(quint64 sent, quint64 total)
{
    // 确保 ui.textEdit 存在并在主线程调用
    
}

void AleacNease::sendLargeFile(QTcpSocket* socket, const QString& filePath)
{
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


//=========================授权区====================================//

void AleacNease::onNewConnection() {
    QTcpSocket* NowSocket = Auther->nextPendingConnection();
    if (!NowSocket) return;
    AutherSocket = NowSocket;
    connect(AutherSocket, &QTcpSocket::readyRead,
        this, &AleacNease::onAuthRead);
	
}

void AleacNease::onAuthRead() {   
    if (!AutherSocket) return;

    QByteArray data = AutherSocket->readAll();

    if (data.isEmpty()) return;

    QString message = QString::fromUtf8(data).trimmed();

    
    if (message.startsWith("#token")) {
        const QStringList parts = message.split('/');
        if (parts[1] == "GETAUTH") {
            if (post_as->structdata.size() == 0) {
				QMessageBox::information(this, "授权失败", "请先登录！");
                return;
            }
            else {			
                this->setWindowFlags(this->windowFlags() | Qt::WindowStaysOnTopHint);
                this->show();
                this->raise();
                this->activateWindow();

                // 100ms后取消置顶（只置顶一次）
                QTimer::singleShot(100, this, [this]() {
                    this->setWindowFlags(this->windowFlags() & ~Qt::WindowStaysOnTopHint);
                    this->show();
                    });
				
                ui.tabWidget->setCurrentIndex(2);
                ui.label_17->setText(QString("你好:%1").arg(QString::fromStdString(post_as->getvalue("main", "Configs", "User"))));
                ui.label_16->setText("目标程序名:"+parts[2]);
                
            }
            

        }
    }
    if (message.startsWith("#UPFILE")) {
        const QStringList parts = message.split("||");
        if (parts[1] == "Aleain") {
            QString filePath = parts[2];
            if (filePath.isEmpty()) return;
            qint64 fileSize = QFileInfo(filePath).size() / 1024;
            

            TaskList.push_back("UP:" + filePath);
            onTaskAdded("上传:" + QFileInfo(filePath).fileName());
            ui.progressBar->setValue(0);

        }
    }
    
}

void AleacNease::on_pushButton_3_clicked() {
    ui.tabWidget->setCurrentIndex(1);
    if (!AutherSocket)return;
    AutherSocket->disconnectFromHost();
}

void AleacNease::on_pushButton_2_clicked() {
	AStruct TokenS;
    TokenS.CreateStruct(AStruct::static_getDir() + "/TempSource/TOKENS.AS", "AUTH", "AUTH", "AUTH", post_as->getvalue("main", "Configs", "User"));
    TokenS.loaddata(AStruct::static_getDir() + "/TempSource/TOKENS.AS");   
    if (!AutherSocket)return;
    AutherSocket->write("#TokenSockets\n" + QByteArray(TokenS.structdata));
    std::filesystem::remove(AStruct::static_getDir() + "/TempSource/TOKENS.AS");
    ui.tabWidget->setCurrentIndex(1);
    AutherSocket->disconnectFromHost();
}

AleacNease::~AleacNease()
{
    
}