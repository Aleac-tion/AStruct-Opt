//Aled V11.2 正式并入AleacNease云生态 
#include "Aledicater.h"
#include <iostream>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <string>
#include <QSystemTrayIcon>
#include <qaction.h>
#include <qmenu.h>
Aledicater::Aledicater(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    //this->setFixedSize(962, 897);
	main_as = new AStruct();
    path_as = new AStruct();
    path_as->loaddata(AStruct::static_getDir() + "/main.as");
    init_tableview();
    init_datas();
    init_tray();
    {
        connect(m_socket, &QTcpSocket::readyRead,
            this, &Aledicater::onReadyRead);
    }
}

void Aledicater::on_button_atten_choice_2_clicked() {
	this->hide(); // 隐藏主窗口
}

void Aledicater::init_tray() {
    QSystemTrayIcon* trayIcon = new QSystemTrayIcon(this);
    QScreen* screen = QGuiApplication::primaryScreen();
    trayIcon->setIcon(QIcon(":/Aledicater/C:/Users/Aleac/Desktop/Aledicater.png"));
    trayIcon->setToolTip("Aledicater APP");
    QMenu* trayIconMenu = new QMenu(this);
    QAction* restoreAction = new QAction("显示主页面", this);
    QAction* quitAction = new QAction("退出主页面", this);
    trayIconMenu->addAction(restoreAction);
    trayIconMenu->addAction(quitAction);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->show();
    connect(restoreAction, &QAction::triggered, [=]() {
        this->show();                    // 显示主窗口
        this->activateWindow();          // 激活窗口（获得焦点）
        });
    connect(quitAction, &QAction::triggered, [=]() {
        // 可选：弹出确认对话框
        int ret = QMessageBox::question(this, "确认", "确定要退出吗？",
            QMessageBox::Yes | QMessageBox::No);
        if (ret == QMessageBox::Yes) {
            QApplication::quit();
        }
        });

}

void Aledicater::ParseAuth(QString message)
{
    Auths.singlesaved(message.toStdString(), AStruct::static_getDir() + "/auth.Astruct");
    Auths.loaddata(AStruct::static_getDir() + "/auth.Astruct");
    UserName = QString::fromStdString(Auths.getvalue("AUTH", "AUTH", "AUTH"));
    AleaCook token_Cook(Auths, "ABCDEFGHJSKLSIWSJA12320SJGLSJASS|TokenSockets");
    token_Cook.Cook();
    m_socket->abort();
    Process_Auth();
}

void Aledicater::Process_Auth()
{
    ui.tabWidget->setCurrentIndex(1);

	//m_socket->disconnectFromHost();
    m_socket->connectToHost("v6.aleac.top", 1234);

    m_socket->write("#cmd/TOKEN/Aledicater/init/" + UserName.toUtf8()+"#datas#"+ QByteArray(main_as->structdata));
}

void Aledicater::on_button_compute_clicked() {
    QItemSelectionModel* sm = ui.tableView->selectionModel();
    QModelIndexList selIdxs = sm->selectedIndexes();
    int selectedRow = selIdxs.isEmpty() ? -1 : selIdxs.first().row();
    if (selectedRow == -1)return;
    bool ok = false;
    double need_num = QInputDialog::getDouble(this, u8"请输入计算的天数", u8"即将计算的天数:", 1.0, 0.0, 1e6, 2, &ok);
    if (!ok) {
        QMessageBox::information(this, u8"取消", u8"已取消输入");
        return;
    }

    // 读取各列数据
    QStandardItem* item_totol = model->item(selectedRow, 1); // 余量
    QStandardItem* item_price = model->item(selectedRow, 2); // 价格（本方法不用）
    QStandardItem* item_per_count = model->item(selectedRow, 3); // 每盒量
    QStandardItem* item_day = model->item(selectedRow, 4); // 日/次
    QStandardItem* item_times = model->item(selectedRow, 5); // 次/量

    float totol = item_totol ? item_totol->text().toFloat() : 0.0f;
    float per_count = item_per_count ? item_per_count->text().toFloat() : 0.0f;
    float day = item_day ? item_day->text().toFloat() : 0.0f;
    float times = item_times ? item_times->text().toFloat() : 0.0f;

    // 参考 on_button_onemoon_clicked 的计算法
    float daily_need = day * times;
    float target_need = daily_need * static_cast<float>(need_num);
    float need_real = target_need - totol; // 目标天数需求 - 当前余量

    QString message;
    if (need_real <= 0.0f) {
        // 余量已足够
        message = u8"当前余量已满足目标天数，不需要购买";
    }
    else {
        int need_box = (per_count > 0.0f) ? static_cast<int>(std::ceil(need_real / per_count)) : 0;
        // 片数按不足一盒的剩余片数显示
        int need_pieces = 0;
        if (per_count > 0.0f) {
            float rem = std::fmod(need_real, per_count);
            // 如果正好整盒，余片为 0
            need_pieces = (rem > 0.0f) ? static_cast<int>(std::ceil(rem)) : 0;
        }
        message = QString(u8"当前到目标天数还需约%1盒，另需%2片").arg(need_box).arg(need_pieces);
    }

    QMessageBox::information(this, u8"计算结果", message);
}

void Aledicater::on_GETAUTH_clicked()
{
    m_socket->connectToHost("127.0.0.1", 11100);
    Auths.loaddata(AStruct::static_getDir() + "/auth.Astruct");
    if (!QFile::exists(QString::fromStdString(AStruct::static_getDir() + "/auth.alcst"))) {
        m_socket->write("#token/GETAUTH/Aledicater");
        return;
    }
    else {
        AleaCook token_Cook(Auths, "ABCDEFGHJSKLSIWSJA12320SJGLSJASS|TokenSockets");
        token_Cook.UnCook();
        UserName = QString::fromStdString(Auths.getvalue("AUTH", "AUTH", "AUTH"));
        ui.tabWidget->setCurrentIndex(1);
        m_socket->abort();
        Process_Auth();



    }
}

void Aledicater::on_GETAUTH_2_clicked()
{
    m_socket->connectToHost("v6c.aleac.top", 1234);

    m_socket->write("#cmd/TOKEN/Aledicater/init/" + UserName.toUtf8() + "#datas#" + QByteArray(main_as->structdata));

}

void Aledicater::onReadyRead()
{
    QString message = QString(m_socket->readAll());

    if (message.isEmpty()) {
        return;
    }

    if (message.startsWith("#TokenSockets")) {
        ParseAuth(message);

    }
    else if (message.startsWith("#key")) {
		
    }

}

void Aledicater::on_button_addon_clicked() {
	model->setItem(model->rowCount(), 0, new QStandardItem("新药物"));
	model->setItem(model->rowCount()-1, 1, new QStandardItem("1"));
	model->setItem(model->rowCount()-1, 2, new QStandardItem("1"));
	model->setItem(model->rowCount()-1, 3, new QStandardItem("1"));
	model->setItem(model->rowCount()-1, 4, new QStandardItem("1"));
	model->setItem(model->rowCount()-1, 5, new QStandardItem("1"));
}

void Aledicater::on_button_clear_clicked() {
	model_pill_price->setRowCount(0);
}

void Aledicater::on_button_delpill_clicked() {
    QItemSelectionModel* sm = ui.tableView->selectionModel();
    QModelIndexList selIdxs = sm->selectedIndexes();
    int selectedRow = selIdxs.isEmpty() ? -1 : selIdxs.first().row();
    if (selectedRow == -1)return;
	QStandardItem* item = model->item(selectedRow, 0);
    QMessageBox::StandardButton reply = QMessageBox::information(this, "确认", QString("确定要彻底删除\"%1\"的数据吗？").arg(item->text()), QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::No) return;
    model->removeRow(selectedRow);
    trigger_save();
    on_button_clear_clicked();
}

void Aledicater::update_listtable(){ 
    model_config->setRowCount(0);
    std::string path = AStruct::static_getDir();
    for (const auto& entry : std::filesystem::directory_iterator(path)) {		
        if (entry.is_regular_file()) {
            QString dir_name = QString::fromStdString(entry.path().filename().string());
            if (dir_name.split(".").last() == "Astruct") {
                AStruct temp_as;
				temp_as.loaddata(entry.path().string());
                model_config->setItem(model_config->rowCount(), 0, new QStandardItem(dir_name));
                model_config->setItem(model_config->rowCount()-1, 1, new QStandardItem(temp_as.getvalue("药物列表", "药物", "描述").c_str()));
                model_config->setItem(model_config->rowCount()-1, 2, new QStandardItem(temp_as.getvalue("药物列表", "药物", "创建时间").c_str()));
            }
        }
    }
}

void Aledicater::on_button_atten_choice_3_clicked() {
    QItemSelectionModel* sm = ui.tableView_2->selectionModel();
    QModelIndexList selIdxs = sm->selectedIndexes();
    int selectedRow = selIdxs.isEmpty() ? -1 : selIdxs.first().row();

    QModelIndex path_table = model_config->index(selectedRow, 0);
    QVariant path_data = model_config->data(path_table, Qt::DisplayRole);


	std::string path = AStruct::static_getDir() + "/" + path_data.toString().toStdString();

	path_as->changeValue("path", "main", "path", path);

    init_datas();
    
}

void Aledicater::on_button_atten_choice_4_clicked() {
    bool ok = false;

    // 药物名 (QString)
    QString medName = QInputDialog::getText(this, u8"输入 - 药物名", u8"药物名:", QLineEdit::Normal, QString(), &ok);
    if (!ok || medName.trimmed().isEmpty()) {
        QMessageBox::information(this, u8"取消", u8"已取消输入");
        return;
    }

    // 剩余总量 (float)
    double totalLeftD = QInputDialog::getDouble(this, u8"输入 - 剩余总量", u8"剩余总量:", 0.0, -1e9, 1e9, 2, &ok);
    if (!ok) {
        QMessageBox::information(this, u8"取消", u8"已取消输入");
        return;
    }

    // 价格 (float)
    double priceD = QInputDialog::getDouble(this, u8"输入 - 价格", u8"价格:", 0.0, -1e9, 1e9, 2, &ok);
    if (!ok) {
        QMessageBox::information(this, u8"取消", u8"已取消输入");
        return;
    }

    // 每盒的数量 (float)
    double perBoxD = QInputDialog::getDouble(this, u8"输入 - 每盒数量", u8"每盒的数量:", 1.0, 0.0, 1e9, 2, &ok);
    if (!ok) {
        QMessageBox::information(this, u8"取消", u8"已取消输入");
        return;
    }

    // 一日几次 (float)
    double perDayD = QInputDialog::getDouble(this, u8"输入 - 一日几次", u8"一日几次:", 1.0, 0.0, 1e6, 2, &ok);
    if (!ok) {
        QMessageBox::information(this, u8"取消", u8"已取消输入");
        return;
    }

    // 每次剂量 (float)
    double perDoseD = QInputDialog::getDouble(this, u8"输入 - 每次剂量", u8"每次剂量:", 1.0, 0.0, 1e9, 2, &ok);
    if (!ok) {
        QMessageBox::information(this, u8"取消", u8"已取消输入");
        return;
    }

    QString TIMESName = QInputDialog::getText(this, u8"输入 - 配置信息", u8"创建时间:", QLineEdit::Normal, QString(), &ok);
    if (!ok) {
        QMessageBox::information(this, u8"取消", u8"已取消输入");
        return;
    }

    QString SCName = QInputDialog::getText(this, u8"输入 - 配置信息", u8"备注:", QLineEdit::Normal, QString(), &ok);
    if (!ok) {
        QMessageBox::information(this, u8"取消", u8"已取消输入");
        return;
    }

    QString confName = QInputDialog::getText(this, u8"输入 - 配置信息", u8"配置文件名:", QLineEdit::Normal, QString(), &ok);
    if (!ok) {
        QMessageBox::information(this, u8"取消", u8"已取消输入");
        return;
    }

    // 将 double 转为 float 并保存到局部变量，供后续使用
    QString  drugName = medName;
    float    leftTotal = static_cast<float>(totalLeftD);
    float    price = static_cast<float>(priceD);
    float    perBoxCount = static_cast<float>(perBoxD);
    float    timesPerDay = static_cast<float>(perDayD);
    float    dosePerTime = static_cast<float>(perDoseD);

    AList list;
    list << (AList()
         << drugName.toStdString()
         << leftTotal
         << price
         << perBoxCount
         << dosePerTime
		 << timesPerDay);

	std::string path = AStruct::static_getDir() + "/" + confName.toStdString() + ".Astruct";
    AStruct create_as;
    create_as.CreateStruct(path, "药物列表", "药物", "name", list.toArray());
    create_as.loaddata(path);
    create_as.addkey("药物列表", "药物", "描述", SCName.toStdString());
    create_as.addkey("药物列表", "药物", "创建时间", TIMESName.toStdString());
    update_listtable();
}

void Aledicater::on_button_atten_choice_clicked() {
    if (model_pill_price->rowCount() == 0) return;
    QMessageBox::StandardButton reply = QMessageBox::information(this, u8"确认", u8"确定对列表中的药物执行签到吗？", QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }

	QString result_summary;

    for (int i = 0; i < model_pill_price->rowCount(); ++i) {
        QModelIndex pre_pill_table = model_pill_price->index(i, 0);
        if (!pre_pill_table.isValid()) {
            model_pill_price->setItem(i, 2, new QStandardItem("未匹配"));
            continue;
        }

        QString prePillName = model_pill_price->data(pre_pill_table, Qt::DisplayRole).toString();
        bool matched = false;

        for (int c = 0; c < model->rowCount(); ++c) {
            QModelIndex pill_table = model->index(c, 0);
            if (!pill_table.isValid()) continue;

            QString pillName = model->data(pill_table, Qt::DisplayRole).toString();
            if (pillName != prePillName) continue;

            // 找到匹配行，读取 model 中各列数据（使用 c 行）
            float totol = model->data(model->index(c, 1), Qt::DisplayRole).toFloat();
            float price = model->data(model->index(c, 2), Qt::DisplayRole).toFloat();
            float per_count = model->data(model->index(c, 3), Qt::DisplayRole).toFloat();
            float day = model->data(model->index(c, 4), Qt::DisplayRole).toFloat();
            float times = model->data(model->index(c, 5), Qt::DisplayRole).toFloat();
            float daily_need = day * times;
            float result = totol - daily_need;
            if (result < 0) {
                result = 0;
            }
            model->setItem(c, 1, new QStandardItem(QString::number(result)));
            break;
        }
        trigger_save();
		result_summary = result_summary  + prePillName + "\n";
    }

    QMessageBox::information(this, "签到结果", QString("已为\n%1完成签到").arg(result_summary));
}

void Aledicater::on_button_time_clicked() {
    for (int i = 0; i < model->rowCount(); i++) {
        QModelIndex name_table = model->index(i, 0);
        QVariant name_data = model->data(name_table, Qt::DisplayRole);
        QModelIndex day_table = model->index(i, 4);
        QVariant day_data = model->data(day_table, Qt::DisplayRole);
        QModelIndex times_table = model->index(i, 5);
        QVariant times_data = model->data(times_table, Qt::DisplayRole);
        QModelIndex total_table = model->index(i, 1);
        QVariant total_data = model->data(total_table, Qt::DisplayRole);
        auto day_float = day_data.toString().toFloat();
        auto times_float = times_data.toString().toFloat();
        auto need_total = day_float * times_float;
        int left_day = total_data.toString().toFloat() / need_total;
        QMessageBox::information(this, "剩余天数", name_data.toString() + "还剩余:" + QString::number(left_day) + "天");
    }
}

void Aledicater::on_button_onemoon_clicked() {
    if (model_pill_price->rowCount() == 0) return;

    double preprice = 0.0;

    for (int i = 0; i < model_pill_price->rowCount(); ++i) {
        QModelIndex pre_pill_table = model_pill_price->index(i, 0);
        if (!pre_pill_table.isValid()) {
            model_pill_price->setItem(i, 2, new QStandardItem("未匹配"));
            continue;
        }

        QString prePillName = model_pill_price->data(pre_pill_table, Qt::DisplayRole).toString();
        bool matched = false;

        for (int c = 0; c < model->rowCount(); ++c) {
            QModelIndex pill_table = model->index(c, 0);
            if (!pill_table.isValid()) continue;

            QString pillName = model->data(pill_table, Qt::DisplayRole).toString();
            if (pillName != prePillName) continue;

            // 找到匹配行，读取 model 中各列数据（使用 c 行）
            float totol = model->data(model->index(c, 1), Qt::DisplayRole).toFloat();
            float price = model->data(model->index(c, 2), Qt::DisplayRole).toFloat();
            float per_count = model->data(model->index(c, 3), Qt::DisplayRole).toFloat();
            float day = model->data(model->index(c, 4), Qt::DisplayRole).toFloat();
            float times = model->data(model->index(c, 5), Qt::DisplayRole).toFloat();

            float daily_need = day * times;
            float monthly_need = daily_need * 30.0f;
            float need_real = monthly_need - totol;

            QString finaldata;
            if (need_real <= 0.0f) {
                finaldata = "不需要购买";
            }
            else {
                int need_box = (per_count > 0.0f) ? static_cast<int>(std::ceil(need_real / per_count)) : 0;
                preprice += static_cast<double>(price) * need_box;
                finaldata = QString("约%1盒(%2片)").arg(need_box).arg(need_real);
            }

            model_pill_price->setItem(i, 2, new QStandardItem(finaldata));
            matched = true;
            break; // 已匹配，跳出内层循环
        }

        if (!matched) {
            model_pill_price->setItem(i, 2, new QStandardItem("未匹配"));
        }
    }

    ui.view_price->setText(QString::number(preprice, 'f', 2));

    const float reimbursementRate = 0.958f;
    double amountAfterInsurance = preprice * (1.0 - reimbursementRate);
    ui.view_medicalir_price->setText(QString::number(amountAfterInsurance, 'f', 2));
}

void Aledicater::on_button_atten_clicked() {
    QMessageBox::StandardButton reply = QMessageBox::information(this, "确认", "确定要执行签到吗？", QMessageBox::Yes | QMessageBox::No);
    if (reply != QMessageBox::Yes) {
        return;
    }
    for (int i = 0; i < model->rowCount(); i++) {
        QModelIndex day_table = model->index(i, 4);
        QVariant day_data = model->data(day_table, Qt::DisplayRole);
        QModelIndex times_table = model->index(i, 5);
        QVariant times_data = model->data(times_table, Qt::DisplayRole);
        QModelIndex total_table = model->index(i, 1);
        QVariant total_data = model->data(total_table, Qt::DisplayRole);
        auto day_float = day_data.toString().toFloat();
        auto times_float = times_data.toString().toFloat();
        auto need_total = day_float * times_float;
        auto total = total_data.toString().toFloat();
        float result = total - need_total;
        if (result < 0) {
            result = 0;
        }
        model->setItem(i, 1, new QStandardItem(QString::number(result)));
        trigger_save();
    }


}

void Aledicater::trigger_save() {
    AList list,lists;
    for (int i = 0; i < model->rowCount(); i++) {
        for (int c = 0; c < model->columnCount(); c++) {
            QModelIndex pill_table = model->index(i, c);
			QVariant  pill_data = model->data(pill_table, Qt::DisplayRole);
			list << pill_data.toString().toStdString();
        }      
		lists << list.toArray();
        list.free();
    }           
	main_as->changeValue("药物列表", "药物","name",lists.toArray());
}

void Aledicater::updatepilltable() {
    ui.view_pills_quan->setText(QString::number(model_pill_price->rowCount()));
}

void Aledicater::on_tableView_clicked(const QModelIndex& index) {
    if (islocked == true) {
        QItemSelectionModel* sm = ui.tableView->selectionModel();
        QModelIndexList selIdxs = sm->selectedIndexes();
        int selectedRow = selIdxs.isEmpty() ? -1 : selIdxs.first().row();
        int rowCount = model_pill_price->rowCount();
        QModelIndex pillname_table = model->index(selectedRow, 0);
        QVariant pillname_data = model->data(pillname_table, Qt::DisplayRole);
        QModelIndex pillprice_table = model->index(selectedRow, 2);
        QVariant pillprice_data = model->data(pillprice_table, Qt::DisplayRole);
        
        bool alreadyExists = false;

        for (int i = 0; i < rowCount; ++i) {
            QModelIndex check_name_table = model_pill_price->index(i, 0);
            if (check_name_table.isValid()) {
                QVariant check_name_data = model_pill_price->data(check_name_table, Qt::DisplayRole);
                std::string compare_a = check_name_data.toString().toStdString();
                if (compare_a == pillname_data.toString().toStdString()) {
                    alreadyExists = true;
                    break;
                }
            }
            else {

            }
        }

        if (!alreadyExists) {
            QStandardItem * mes = new QStandardItem(pillname_data.toString());
            mes->setFlags(mes->flags() & ~Qt::ItemIsEditable);       
            model_pill_price->setItem(rowCount, 0, mes);
            mes = new QStandardItem(pillprice_data.toString());
            mes->setFlags(mes->flags() & ~Qt::ItemIsEditable);
            model_pill_price->setItem(rowCount, 1, mes);   
            model_pill_price->setItem(rowCount, 2, new QStandardItem(""));
            updatepilltable();
        }
    }
}

void Aledicater::on_button_locktable_clicked() {
    if (islocked == true) {
        islocked = false;
        ui.button_locktable->setText("锁定配置");
        ui.tableView->setEditTriggers(QAbstractItemView::DoubleClicked);
    }
    else {
        islocked = true;
        ui.button_locktable->setText("取消锁定");
        ui.tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        trigger_save();
    }
}

void Aledicater::init_datas() {
    model->setRowCount(0);
    model_pill_price->setRowCount(0);
    
    main_as->loaddata(path_as->getvalue("path", "main", "path"));
    
    AList list;

    /* {
        HMODULE hDll = LoadLibraryA("K:/CPP_VS2026/lib/AStructLan/x64/Release/AStructLan.dll");
        typedef int(*alists)(const char* ID);
        static alists ALists = nullptr;
        ALists = (alists)GetProcAddress(hDll, "ALists");

        typedef const char* (*alistget)(const char* AListID, int index);
        static alistget AList_Get = nullptr;
		AList_Get = (alistget)GetProcAddress(hDll, "AList_Get");

        typedef const char* (*go)(const char* vec, int index);
        static go AList_Go = nullptr;
        AList_Go = (go)GetProcAddress(hDll, "AList_Go");

        typedef const char* (*alistparse)(const char* ID, int index);
        static alistparse AList_Parse = nullptr;
        AList_Parse = (alistparse)GetProcAddress(hDll, "AList_Get");

        typedef const char* (*alistfetch)(const char* ID, const char* vec);
        static alistfetch AList_fetch = nullptr;
        AList_fetch = (alistfetch)GetProcAddress(hDll, "AList_fetch");

        typedef int (*alistsize)(const char* vec);
        static alistsize AList_Size = nullptr;
        AList_Size = (alistsize)GetProcAddress(hDll, "AList_Size");

        std::string drug_name = main_as->getvalue("药物列表", "药物", "name"); //此步骤为持久化超级数组，本身你也可以用AStructLan的getvalue读取，但为了省事就先利用AStruct本体

        static const char* arrays = drug_name.c_str();

        static const char* arrayStr = arrays;
        int outerSize = AList_Size(arrayStr);

        for (int i = 0; i < outerSize; ++i) {
            const char* innerArrayStr = AList_Go(arrayStr, i);
            int innerSize = AList_Size(innerArrayStr);

            // 给内层数组临时分配一个 ID
            std::string innerId = "inner_" + std::to_string(i);
            ALists(innerId.c_str());                  // 确保存在
            AList_fetch(innerId.c_str(), innerArrayStr);

            for (int c = 0; c < innerSize; ++c) {
                const char* val = AList_Get(innerId.c_str(), c);
                model->setItem(i, c, new QStandardItem(val));
            }
        }

       
    }*/
    list = AList::autoParse(main_as->getvalue("药物列表", "药物", "name"));
     {
        for (int i = 0; i < list.size(); i++) {
            for (int c = 0; c < list[i].Go().size(); c++) {
                model->setItem(i, c, new QStandardItem(((std::string)list[i].Go()[c]).c_str()));
            }
        }
     }
    update_listtable();
}

void Aledicater::init_tableview()
{
    model->setHorizontalHeaderItem(0, new QStandardItem("药物名"));
    model->setHorizontalHeaderItem(1, new QStandardItem("余量"));
    model->setHorizontalHeaderItem(2, new QStandardItem("价格"));
    model->setHorizontalHeaderItem(3, new QStandardItem("每盒量"));
    model->setHorizontalHeaderItem(4, new QStandardItem("日/次"));
    model->setHorizontalHeaderItem(5, new QStandardItem("次/量"));
	ui.tableView->setModel(model);
    ui.tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui.tableView->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui.tableView->horizontalHeader()->resizeSection(0, 100);
    ui.tableView->horizontalHeader()->resizeSection(1, 55);
    ui.tableView->horizontalHeader()->resizeSection(2, 55);
    ui.tableView->horizontalHeader()->resizeSection(3, 55);
    ui.tableView->horizontalHeader()->resizeSection(4, 50);
    ui.tableView->horizontalHeader()->resizeSection(5, 50);


    model_pill_price->setHorizontalHeaderItem(0, new QStandardItem("药物名"));
    model_pill_price->setHorizontalHeaderItem(1, new QStandardItem("价格"));
    model_pill_price->setHorizontalHeaderItem(2, new QStandardItem("数量/片剂或针剂"));
    ui.pillstable->horizontalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    ui.pillstable->setModel(model_pill_price);


    model_config->setHorizontalHeaderItem(0, new QStandardItem("配置名"));
    model_config->setHorizontalHeaderItem(1, new QStandardItem("备注"));
    model_config->setHorizontalHeaderItem(2, new QStandardItem("时间"));
    ui.tableView_2->setModel(model_config);
    ui.tableView_2->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui.tableView_2->setSelectionMode((QAbstractItemView::SingleSelection));
    ui.tableView_2->horizontalHeader()->resizeSection(0, 280);
    ui.tableView_2->horizontalHeader()->resizeSection(1, 251);
    ui.tableView_2->horizontalHeader()->resizeSection(2, 150);

}

Aledicater::~Aledicater()
{}

