#include "AAICenter.h"
#include <QMessageBox>
#include <QFileInfo>
#include <QDIr>
#include <QFile>
#include <QPushbutton>

QString GetWindowDirectory(HWND hWnd) {
    if (!hWnd) return QString();

    DWORD processId = 0;
    GetWindowThreadProcessId(hWnd, &processId);
    if (processId == 0) return QString();

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!hProcess) return QString();

    wchar_t path[MAX_PATH] = { 0 };
    DWORD size = MAX_PATH;
    BOOL success = QueryFullProcessImageNameW(hProcess, 0, path, &size);

    CloseHandle(hProcess);

    if (!success) return QString();

    QString fullPath = QString::fromWCharArray(path);
    QFileInfo info(fullPath);
    return info.absolutePath();  
}

HWND FindAAICenterWindow() {
    // 方法1：通过窗口类名查找
    HWND hWnd = FindWindowA(nullptr, "AAICenter");

    if (hWnd) {
		QMessageBox::information(nullptr, "提示", "找到 AAICenter 窗口，句柄: " + QString::number((quintptr)hWnd, 16));
    }
    else {
		QMessageBox::warning(nullptr, "提示", "未找到 AAICenter 窗口");
    }

    return hWnd;
}

AAICenter::AAICenter(QWidget *parent)
    : QWidget(parent)
	, as(new AStruct())
{
    ui.setupUi(this);
    this->setWindowTitle("AAICenter");

    connect(Main_TCP, &QTcpServer::newConnection, this, &AAICenter::onNewConnection);
    if (Main_TCP->listen(QHostAddress::LocalHost, 2455)) {
        quint16 port = Main_TCP->serverPort();
        as->singlesaved(QString::number(port).toStdString(), AStruct::static_getDir() + "/port.as");
    }
    else {
		QMessageBox::warning(this, "错误", "启动异常\n可能源于端口分配(概率极低)\n也有可能是遇到了拦截");
        ///
    }

    {
        plugins_model->setHorizontalHeaderItem(0, new QStandardItem("插件名"));
        plugins_model->setHorizontalHeaderItem(1, new QStandardItem("版本"));
		plugins_model->setHorizontalHeaderItem(2, new QStandardItem("描述"));

		ui.tableView->setModel(plugins_model);
		ui.tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
		ui.tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);

    }
    //FindAAICenterWindow();
    {
		ui.pluginContainer->setGeometry(QRect(460, 80, 531, 411));
        
    }
}

void AAICenter::on_pushButton_2_clicked() {
    QString path  = QString::fromStdString(AStruct::static_getDir()+"/Plugins");
    QDir dir(path);
	AStruct signastruct;
    for (const QFileInfo& fileInfo : dir.entryInfoList(QDir::Files)) {
		if (fileInfo.fileName().endsWith(".dll", Qt::CaseInsensitive)) {
            HMODULE hDll = LoadLibraryA(fileInfo.absoluteFilePath().toStdString().c_str());
            if (!hDll) {
                plugins_model->appendRow(new QStandardItem(fileInfo.fileName()));
				continue;
                hDll = nullptr;
            }

            typedef std::string(*Sign)();
            static Sign Signature = nullptr;
            Sign sign = (Sign)GetProcAddress(hDll, "Signature");
			signastruct.structdata = sign();
            plugins_model->appendRow({
                new QStandardItem(fileInfo.absoluteFilePath()),
                new QStandardItem(signastruct.getvalue("Signature", 0, "Version").c_str()),
                new QStandardItem(signastruct.getvalue("Signature", 0, "Description").c_str())
				});
            if (hDll == nullptr) {
                FreeLibrary(hDll);
            }
            
        }
    }

	/*HMODULE hDll = LoadLibraryA(path);
    typedef int(*MAIN)();
    static MAIN MAINS = nullptr;
	MAINS = (MAIN)GetProcAddress(hDll, "MAINS");
    if (MAINS) {
		QMessageBox::information(this, "提示", "已刷新");
        MAINS();
    }
    if (hDll) {
        FreeLibrary(hDll);
	}*/
}

void AAICenter::on_tableView_clicked() {
	
    QItemSelectionModel* sm = ui.tableView->selectionModel();
    QModelIndexList selIdxs = sm->selectedIndexes();
    int selectedRow = selIdxs.isEmpty() ? -1 : selIdxs.first().row();

    if (selectedRow != -1) {
		QStandardItem* nameItem = plugins_model->item(selectedRow, 0);
		QString pluginPath = nameItem->text();
        Parse_Plugins(pluginPath);
    }


}

void AAICenter::Parse_Plugins(QString& plugins) {
    QMessageBox::StandardButton reply = QMessageBox::information(this, "AAIScript", "确定加载此插件吗？", QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }
	hDll = LoadLibraryA(plugins.toStdString().c_str());
	if (!hDll) {
        QMessageBox::warning(this, "错误", "加载插件失败！");
        return;
    }

    typedef int(*MAIN)();
    static MAIN MAINS = nullptr;
    MAINS = (MAIN)GetProcAddress(hDll, "MAINS");
    if (MAINS) {
        MAINS();
    }

    
}

void AAICenter::addonlineedit(AStruct& datas) {
    QString text = QString::fromStdString(datas.getvalue("AddonLineEdit", "metal", "Text"));
    if (button_list.contains(text)) {
        ui.textEdit->append("不允许出现两个相同名称的输入框！");
        return;
	}
	lineedit_list[text] = new QLineEdit(ui.pluginContainer);
	auto line = lineedit_list[text];
	float x = std::stof(datas.getvalue("AddonLineEdit", "metal", "x"));
	float y = std::stof(datas.getvalue("AddonLineEdit", "metal", "y"));
    float width = std::stof(datas.getvalue("AddonLineEdit", "metal", "width"));
	float height = std::stof(datas.getvalue("AddonLineEdit", "metal", "height"));

	line->setGeometry(x, y, width, height);
    line->show();

    
}

void AAICenter::addonbutton(AStruct& datas) {
    QString text = QString::fromStdString(datas.getvalue("AddonButton", "metal", "Text"));
    if (button_list.contains(text)) {
        ui.textEdit->append("不允许出现两个相同名称的按钮！");
        return;
    }

    button_list[text] = new QPushButton(text.toStdString().c_str(), ui.pluginContainer);
    auto btn = button_list[text];
	float x = std::stof(datas.getvalue("AddonButton", "metal", "x"));
	float y = std::stof(datas.getvalue("AddonButton", "metal", "y"));
	float width = std::stof(datas.getvalue("AddonButton", "metal", "width"));
	float height = std::stof(datas.getvalue("AddonButton", "metal", "height"));

	btn->setGeometry(x, y, width, height);
    btn->show();
    connect(btn, &QPushButton::clicked, [this, text]() {
        AStruct* eventData = new AStruct();
        eventData->AppendTitle("Event", "Buttons", "type", "click");
        eventData->addkey("Event", "Buttons", "name", text.toStdString());

        typedef void(*Trigger)(AStruct*);
        Trigger Trigger_Event = (Trigger)GetProcAddress(hDll, "Trigger_Event");
        if (!Trigger_Event) { delete eventData; return; }

        // 关键：不要阻塞 UI 线程
        QtConcurrent::run([Trigger_Event, eventData]() {
            Trigger_Event(eventData);
            delete eventData;
            });
        });
}

void AAICenter::addontextedit(AStruct& datas) {
    QString text = QString::fromStdString(datas.getvalue("AddonTextEdit", "metal", "Text"));
    if (textedit_list.contains(text)) {
        ui.textEdit->append("不允许出现两个相同名称的文本编辑框！");
        return;
    }
    textedit_list[text] = new QTextEdit(ui.pluginContainer);
    auto textEdit = textedit_list[text];
    float x = std::stof(datas.getvalue("AddonTextEdit", "metal", "x"));
    float y = std::stof(datas.getvalue("AddonTextEdit", "metal", "y"));
    float width = std::stof(datas.getvalue("AddonTextEdit", "metal", "width"));
    float height = std::stof(datas.getvalue("AddonTextEdit", "metal", "height"));

    textEdit->setGeometry(x, y, width, height);
    textEdit->show();
}

void AAICenter::on_pushButton_clicked() {
    HWND hWnd = FindWindowA(nullptr, "AAICenter");
    if (hWnd) {
        QString dir = GetWindowDirectory(hWnd);

		QMessageBox::information(this, "AAICenter 运行目录", dir);
        qDebug() << "AAICenter 运行目录:" << dir;
    }
    else {
        qDebug() << "未找到 AAICenter";
    }
}

void AAICenter::onNewConnection() {
    QTcpSocket* clientSocket = Main_TCP->nextPendingConnection();
    m_clients.append(clientSocket);
    connect(clientSocket, &QTcpSocket::readyRead, this, &AAICenter::onReadyRead);
    ui.textEdit->append("被连接");
}

void AAICenter::onReadyRead() {
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    if (socket->state() != QAbstractSocket::ConnectedState) {
        // 已经不是连接状态，不再处理任何数据，防止读到乱流
        return;
    }

    m_recvBuffer.append(socket->readAll());

    while (true) {
        // 至少需要 4 字节才能读长度
        if (m_recvBuffer.size() < 4) break;

        // 读取长度头（网络字节序转主机字节序）
        uint32_t msgLen = qFromBigEndian<quint32>(
            reinterpret_cast<const uchar*>(m_recvBuffer.constData())
        );

        // 检查消息是否完整
        if (m_recvBuffer.size() < 4 + msgLen) break;

        // 提取消息体
        QByteArray msgBody = m_recvBuffer.mid(4, msgLen);

        // 处理这条完整消息
        processMessage(msgBody);

        // 从缓冲区移除已处理的部分
        m_recvBuffer.remove(0, 4 + msgLen);
    }
}

void AAICenter::processMessage(const QByteArray& msgBody) {
    ui.textEdit->append("收到消息: " + QString::fromUtf8(msgBody));

    if (msgBody.startsWith("#ASSign")) {
        AStruct RootAS;
        RootAS.structdata = msgBody.toStdString();
        AList TitleList = RootAS.GetAllTitle();
        if ((std::string)TitleList[0] == "MessageBox") {
            processMessageBOX(RootAS);
              }
        else if ((std::string)TitleList[0] == "Signature") {

        }
        else if ((std::string)TitleList[0] == "AddonButton") {
            addonbutton(RootAS);
        }
        else if ((std::string)TitleList[0] == "logs") {
            ui.textEdit->append(("[Log-output]:" + RootAS.getvalue("logs", "metal", "msg")).c_str());
        }
        else if ((std::string)TitleList[0] == "AddonLineEdit") {
            addonlineedit(RootAS);
        }
        else if ((std::string)TitleList[0] == "ReceiveEvent") {
            ProcessReceiveEvent(RootAS);
        }
        else if ((std::string)TitleList[0] == "Layouts") {
            ProcessLayoutsEvent(RootAS);
        }
        else if ((std::string)TitleList[0] == "AddonTextEdit") {
            addontextedit(RootAS);
        }
    }
}

void AAICenter::processMessageBOX(AStruct& data) {

    if ((std::string)data.GetHeadersFromTitle("MessageBox")[1] == "metal") {
        if ((std::string)data.GetKeysFromHeader("MessageBox", "Method")[0] == "INFO") {
            QMessageBox::information(nullptr, data.getvalue("MessageBox", 0, "title").c_str(), data.getvalue("MessageBox", 0, "msg").c_str());
        }
        else if((std::string)data.GetKeysFromHeader("MessageBox", "Method")[0]=="GetInput"){
            bool ok;
            QString text = QInputDialog::getText(nullptr, data.getvalue("MessageBox", 0, "title").c_str(), data.getvalue("MessageBox", 0, "msg").c_str(), QLineEdit::Normal, "", &ok);
            if (ok) {
                AStruct res;
                res.AppendTitle("MessageBox", "event", "Input", text.toStdString());
                typedef void(*Rev)(AStruct*);
                static Rev ReceiveEvent = nullptr;
                ReceiveEvent = (Rev)GetProcAddress(hDll, "ReceiveEvent");
                if (!ReceiveEvent) {
                    ui.textEdit->append("未找到 ReceiveEvent 导出函数");
                    return;
                }
                ReceiveEvent(&res);
            }
		}
    }
    
}

void AAICenter::ProcessLayoutsEvent(AStruct& data) {
    if (data.getvalue("Layouts", "metal", "type") == "LineEdit") {
        QString name = data.getvalue("Layouts", "metal", "name").c_str();
        if (!lineedit_list.contains(name)) {
            ui.textEdit->append("未找到布局中指定的 LineEdit: " + name);
            return;
        }
        else
        {
            if ((std::string)data.GetHeadersFromTitle("Layouts")[0] == "Method") {
                auto line = lineedit_list[name];
                if ((std::string)data.GetKeysFromHeader("Layouts", "Method")[0] == "SetText") {
                    line->setText(data.getvalue("Layouts", "Method", "SetText").c_str());
                }
                else if ((std::string)data.GetKeysFromHeader("Layouts", "Method")[0] == "AppendText") {
                    line->setText(line->text() + data.getvalue("Layouts", "Method", "AppendText").c_str());
                }
                else if ((std::string)data.GetKeysFromHeader("Layouts", "Method")[0] == "OnlyRead") {
                    bool readonly = (data.getvalue("Layouts", "Method", "OnlyRead") == "true") ? true : false;
                    line->setReadOnly(readonly);
                }
                else if ((std::string)data.GetKeysFromHeader("Layouts", "Method")[0] == "Clear"){
                    line->clear();
					line->setText("");
                }
            }

        }
    }
}

void AAICenter::ProcessReceiveEvent(AStruct &data) {
    if ((std::string)data.GetAllTitle()[0] == "ReceiveEvent") { 
        typedef void(*Rev)(AStruct*);
        static Rev ReceiveEvent = nullptr;
        ReceiveEvent = (Rev)GetProcAddress(hDll, "ReceiveEvent");
        if (!ReceiveEvent) {
            ui.textEdit->append("未找到 ReceiveEvent 导出函数");
            return;
        }
        if (data.getvalue("ReceiveEvent", "metal", "type") == "LineEdit") {
            if (data.getvalue("ReceiveEvent", "metal", "Method") == "GetText") {
				std::string name = data.getvalue("ReceiveEvent", "metal", "name");		
                AStruct res;
                if (lineedit_list[name.c_str()]->text().isEmpty()) {
					res.AppendTitle("LineEdit", "event", "Text", "NULLPTR_TEXT_EMPTY");
                    ReceiveEvent(&res);
                }
                else {
                    res.AppendTitle("LineEdit", "event", "Text", lineedit_list[name.c_str()]->text().toStdString());
                    ReceiveEvent(&res);
                }
                
            }
        }
        if (data.getvalue("ReceiveEvent", "metal", "type") == "MessageBox") {
			ui.textEdit->append("收到 MessageBox 事件");
			if (data.getvalue("ReceiveEvent", "Method", "GetInput") == "string") {
                ui.textEdit->append("收到 INPUT 事件");
                bool ok;
                QString text = QInputDialog::getText(nullptr, data.getvalue("ReceiveEvent", "metal", "title").c_str(), data.getvalue("ReceiveEvent", "metal", "msg").c_str(), QLineEdit::Normal, "", &ok);
                if (ok) {
                    AStruct res;
                    if(text.toStdString() == "") {
                        res.AppendTitle("MessageBox", "Input", "Text", "NULLPTR_TEXT_EMPTY");
                        ReceiveEvent(&res);
                        return;
					}

                    res.AppendTitle("MessageBox", "Input", "Text", text.toStdString());
                    ReceiveEvent(&res);
                }
                else {
                    AStruct res; 
                    res.AppendTitle("MessageBox", "Input", "Text", "NULLPTR_TEXT_EMPTY");
                    ReceiveEvent(&res);
                }
            }
        }
    }
    else {
        return;
    }
    
    

    
}

AAICenter::~AAICenter()
{
    delete as;

}

