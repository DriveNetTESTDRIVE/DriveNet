#include <qt/confgeneratordialog.h>

#include <qt/forms/ui_confgeneratordialog.h>

#include <fs.h>
#include <random.h>
#include <sidechain.h>
#include <util.h>

#include <QFile>
#include <QMessageBox>
#include <QTextStream>

ConfGeneratorDialog::ConfGeneratorDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConfGeneratorDialog)
{
    ui->setupUi(this);
}

ConfGeneratorDialog::~ConfGeneratorDialog()
{
    delete ui;
}

void ConfGeneratorDialog::on_pushButtonClose_clicked()
{
    this->close();
}

void ConfGeneratorDialog::on_pushButtonApply_clicked()
{
    QMessageBox messageBox;
    if (ui->lineEditUser->text().isEmpty()) {
        messageBox.setIcon(QMessageBox::Critical);
        messageBox.setWindowTitle("Invalid RPC username");
        messageBox.setText("You must enter an RPC username!");
        messageBox.exec();
        return;
    }

    if (ui->lineEditPassword->text().isEmpty()) {
        messageBox.setIcon(QMessageBox::Critical);
        messageBox.setWindowTitle("Invalid RPC password");
        messageBox.setText("You must enter an RPC password!");
        messageBox.exec();
        return;
    }

    if (WriteConfigFiles(ui->lineEditUser->text(), ui->lineEditPassword->text())) {
        messageBox.setIcon(QMessageBox::Information);
        messageBox.setWindowTitle("Configuration files created!");
        QString str = "Configuration files created!\n\n";
        str += "You must restart DriveNet and any\n";
        str += "sidechains for changes to be applied.";
        messageBox.setText(str);
        messageBox.exec();

        this->close();
    }
}

void ConfGeneratorDialog::on_pushButtonRandom_clicked()
{
    std::string strSeed = GetRandHash().ToString();

    std::string strUser = strSeed.substr(0, 14);
    std::string strPass = strSeed.substr(15, 31);

    ui->lineEditUser->setText(QString::fromStdString(strUser));
    ui->lineEditPassword->setText(QString::fromStdString(strPass));
}

bool ConfGeneratorDialog::WriteConfigFiles(const QString& strUser, const QString& strPass)
{
    QMessageBox messageBox;
    messageBox.setWindowTitle("Error writing config files!");
    messageBox.setIcon(QMessageBox::Critical);

    // Make sure we have the mainchain data directory
    fs::path pathDriveNet = GetDefaultDataDir();
    if (!fs::exists(pathDriveNet)) {
        // Show error message
        messageBox.setText("Could not find DriveNet directory!");
        messageBox.exec();
        return false;
    }

    fs::path pathHome = GetHomeDir();

    // For the current valid sidechains
    for (const std::pair<std::string, std::string>& p : ConfigDirectories) {
        // Does the directory exist?
        fs::path pathData = pathHome / p.first;
        if (!fs::exists(pathData))
            continue;

        // Do we need to backup the old config file?
        fs::path pathConf = pathData / p.second;
        if (fs::exists(pathConf)) {
            // Rename old configuration file
            QString strNew = QString::fromStdString(pathConf.string());
            strNew += ".OLD";

            // Make sure that we moved it
            if (!QFile::rename(QString::fromStdString(pathConf.string()), strNew)) {
                QString strError = "You must first remove ";
                strError += strNew;
                strError += ".\n";
                messageBox.setText(strError);
                messageBox.exec();
                return false;
            }
        }

        if (fs::exists(pathConf)) {
            QString strError = "Failed to rename: ";
            strError += QString::fromStdString(pathConf.string());
            strError += "!\n";
            messageBox.setText(strError);
            messageBox.exec();

            // We failed to rename the conf file
            return false;
        }

        // Write new configuration file
        QFile file(QString::fromStdString(pathConf.string()));
        if (!file.open(QIODevice::ReadWrite | QIODevice::Text)) {
            QString strError = "Error while opening for write: ";
            strError += QString::fromStdString(pathConf.string());
            strError += "!\n";
            messageBox.setText(strError);
            messageBox.exec();
            return false;
        }

        QTextStream out(&file);
        out << "rpcuser=";
        out << strUser;
        out << "\n";
        out << "rpcpassword=";
        out << strPass;
        out << "\n";

        if (p.first == ConfigDirectories[0].first)
            out << "server=1\n";

        file.close();
    }

    return true;
}
