// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2016-2017 The Karbowanec developers
// Copyright (c) 2017-2018 The Brazukcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <QApplication>
#include <QCommandLineParser>
#include <QLocale>
#include <QLockFile>
#include <QMessageBox>
#include <QSplashScreen>
#include <QStyleFactory>
#include <QTextCodec>
#include <QTranslator>

#include "gui/MainWindow.h"
#include "CommandLineParser.h"
#include "CurrencyAdapter.h"
#include "LoggerAdapter.h"
#include "NodeAdapter.h"
#include "Settings.h"
#include "SignalHandler.h"
#include "WalletAdapter.h"
#include "PaymentServer.h"
#include "Update.h"

#define DEBUG 1


int main(int argc, char* argv[])
{
  QApplication app(argc, argv);
  app.setApplicationName(WalletGui::CurrencyAdapter::instance().getCurrencyName() + "wallet");
  app.setApplicationVersion(WalletGui::Settings::instance().getVersion());
  app.setQuitOnLastWindowClosed(false);

  #ifndef Q_OS_MAC
  QApplication::setStyle(QStyleFactory::create("Fusion"));
  #endif

  WalletGui::CommandLineParser cmdLineParser(nullptr);
  WalletGui::Settings::instance().setCommandLineParser(&cmdLineParser);
  bool cmdLineParseResult = cmdLineParser.process(app.arguments());
  
  WalletGui::Settings::instance().load();
  
  QTranslator translator;
  QTranslator translatorQt;
  QString lng = WalletGui::Settings::instance().getLanguage();

  if(!lng.isEmpty()) {
    translator.load(":/languages/" + lng + ".qm");
    translatorQt.load(":/languages/qt_" + lng + ".qm");

    if(lng == "cz") {
      QLocale::setDefault(QLocale("cz"));
    }
    else if(lng == "de") {
      QLocale::setDefault(QLocale("de"));
    }
    else if(lng == "en-PT") {
      QLocale::setDefault(QLocale("en-PT"));
    }
    else if(lng == "es-ES") {
      QLocale::setDefault(QLocale("es-ES"));
    }
    else if(lng == "fr") {
      QLocale::setDefault(QLocale("fr"));
    }
    else if(lng == "id") {
      QLocale::setDefault(QLocale("id"));
    }
    else if(lng == "it") {
      QLocale::setDefault(QLocale("it"));
    }
    else if(lng == "kab") {
      QLocale::setDefault(QLocale("kab"));
    }
    else if(lng == "pt-BR") {
      QLocale::setDefault(QLocale("pt-BR"));
    }
    else if(lng == "ru") {
      QLocale::setDefault(QLocale("ru"));
    }
    else if(lng == "vi") {
      QLocale::setDefault(QLocale("vi"));
    }
    else if(lng == "zh-CN") {
      QLocale::setDefault(QLocale("zh-CN"));
    }
    else {
      QLocale::setDefault(QLocale::c());
    }

  } else {
    translator.load(":/languages/" + QLocale::system().name());
    translatorQt.load(":/languages/qt_" +  QLocale::system().name());
    QLocale::setDefault(QLocale::system().name());
  }
  
  app.installTranslator(&translator);
  app.installTranslator(&translatorQt);

  //QLocale::setDefault(QLocale::c());

  //QLocale locale = QLocale("uk_UA");
  //QLocale::setDefault(locale);

  setlocale(LC_ALL, "");

  QFile File(":/skin/default.qss");
  File.open(QFile::ReadOnly);
  QString StyleSheet = QLatin1String(File.readAll());
  qApp->setStyleSheet(StyleSheet);

  if (PaymentServer::ipcSendCommandLine())
  exit(0);

  PaymentServer* paymentServer = new PaymentServer(&app);

#ifdef Q_OS_WIN
  if(!cmdLineParseResult) {
    QMessageBox::critical(nullptr, QObject::tr("Error"), cmdLineParser.getErrorText());
    return app.exec();
  } else if (cmdLineParser.hasHelpOption()) {
    QMessageBox::information(nullptr, QObject::tr("Help"), cmdLineParser.getHelpText());
    return app.exec();
  }
#endif

  WalletGui::LoggerAdapter::instance().init();

  QString dataDirPath = WalletGui::Settings::instance().getDataDir().absolutePath();

  if (!QDir().exists(dataDirPath)) {
    QDir().mkpath(dataDirPath);
  }

  QLockFile lockFile(WalletGui::Settings::instance().getDataDir().absoluteFilePath(QApplication::applicationName() + ".lock"));
  if (!lockFile.tryLock()) {
    QMessageBox::warning(nullptr, QObject::tr("Fail"), QObject::tr("%1 wallet already running").arg(WalletGui::CurrencyAdapter::instance().getCurrencyDisplayName()));
    return 0;
  }

  WalletGui::SignalHandler::instance().init();
  QObject::connect(&WalletGui::SignalHandler::instance(), &WalletGui::SignalHandler::quitSignal, &app, &QApplication::quit);

  QSplashScreen* splash = new QSplashScreen(QPixmap(":images/splash"), /*Qt::WindowStaysOnTopHint |*/ Qt::X11BypassWindowManagerHint);
  if (!splash->isVisible()) {
    splash->show();
  }

  splash->showMessage(QObject::tr("Loading blockchain..."), Qt::AlignLeft | Qt::AlignBottom, Qt::white);

  app.processEvents();
  qRegisterMetaType<CryptoNote::TransactionId>("CryptoNote::TransactionId");
  qRegisterMetaType<quintptr>("quintptr");
  if (!WalletGui::NodeAdapter::instance().init()) {
    return 0;
  }
  splash->finish(&WalletGui::MainWindow::instance());
  Updater d;
  //d.checkForUpdate();  // FIX-ME
  WalletGui::MainWindow::instance().show();
  WalletGui::  WalletAdapter::instance().open("");

  QTimer::singleShot(1000, paymentServer, SLOT(uiReady()));
  QObject::connect(paymentServer, &PaymentServer::receivedURI, &WalletGui::MainWindow::instance(), &WalletGui::MainWindow::handlePaymentRequest, Qt::QueuedConnection);

  QObject::connect(QApplication::instance(), &QApplication::aboutToQuit, []() {
    WalletGui::MainWindow::instance().quit();
    if (WalletGui::WalletAdapter::instance().isOpen()) {
      WalletGui::WalletAdapter::instance().close();
    }

    WalletGui::NodeAdapter::instance().deinit();
  });

  return app.exec();
}
