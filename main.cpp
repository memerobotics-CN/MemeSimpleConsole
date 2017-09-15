#include <QCoreApplication>

#include <QtSerialPort/QSerialPort>
#include <QDateTime>
#include <QThread>
#include <QTimer>

#include <iostream>
#include <QDebug>


#include "MemeServoAPI/MemeServoAPI.h"


QSerialPort serialPort;

volatile bool should_exit;

void signal_handler(int signum)
{
   should_exit = true;
   printf("Caught signal %d, coming out...\n", signum);
}

void DelayMilisecondImpl(uint32_t ms)
{
    uint64_t endTime = QDateTime::currentMSecsSinceEpoch() + ms;
    while (endTime > (uint64_t)QDateTime::currentMSecsSinceEpoch())
    {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
}


uint32_t GetMilliSecondsImpl()
{
    static uint64_t startTime = QDateTime::currentMSecsSinceEpoch();

    return (uint32_t)(QDateTime::currentMSecsSinceEpoch() - startTime);
}


void SendDataImpl(uint8_t addr, uint8_t *data, uint8_t size)
{
    Q_UNUSED(addr);

    serialPort.write((const char*)data, size);

    char buf[size * 5 + 1];

    for (int i=0; i<size; i++)
    {
        sprintf(buf + i * 5, " 0x%02X", data[i]);
    }
    //qDebug() << "Send to device: " << buf;
}


void RecvDataImpl()
{
    char c;

    while (serialPort.getChar(&c))
    {
        //qDebug() << "recv from device: " << (unsigned char)c;
        MMS_OnData(c);
    }
}


void OnNodeError(unsigned char node_addr, unsigned char err)
{
    // Dispaly
    std::cout << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz").toStdString() << " - node: " << (int)node_addr << ", error: " << (int)err << std::endl;
}


int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    if (argc != 3)
    {
        std::cout << "Missing parameters."  << std::endl << "Usage: " << std::endl << "SampleConsoleQT <COMx> <baudrate>" << std::endl << std::endl;
    }
    else
    {
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
        signal(SIGABRT, signal_handler);

        serialPort.setPortName(argv[1]);
        serialPort.setBaudRate(QString(argv[2]).toInt());

        if (serialPort.open(QIODevice::ReadWrite))
        {
            std::cout << "Port '" << argv[1] << "' opened, baud rate " << argv[2] << "." << std::endl;
        }
        else
        {
            std::cout << "Port '" << argv[1] << "' open failed." << std::endl;
            return -1;
        }

        MMS_SetProtocol(MMS_PROTOCOL_UART, 0x01/*master address*/, SendDataImpl, RecvDataImpl);
        MMS_SetTimerFunction(GetMilliSecondsImpl, DelayMilisecondImpl);
        MMS_SetCommandTimeOut(100);

        uint8_t ret;

        while ((ret = MMS_StartServo((uint8_t)2, 0, OnNodeError)) != MMS_RESP_SUCCESS)
        {
            std::cout << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz").toStdString() << " MMS_StartServo failed: " << (int)ret << ", node: " << (int)2 << std::endl;
        }
        while ((ret = MMS_StartServo((uint8_t)3, 0, OnNodeError)) != MMS_RESP_SUCCESS)
        {
            std::cout << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz").toStdString() << " MMS_StartServo failed: " << (int)ret << ", node: " << (int)3 << std::endl;
        }

        should_exit = false;

        while (!should_exit)
        {
            uint8_t status, in_position;

            while ((ret = MMS_GetControlStatus((uint8_t)2, &status, &in_position, OnNodeError)) != MMS_RESP_SUCCESS)
            {
                std::cout << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz").toStdString() << " MMS_GetControlStatus failed: " << (int)ret << ", node: " << (int)2 << std::endl;
            }

            //DelayMilisecondImpl(500);
            //QCoreApplication::processEvents();

            while ((ret = MMS_GetControlStatus((uint8_t)3, &status, &in_position, OnNodeError)) != MMS_RESP_SUCCESS)
            {
                std::cout << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz").toStdString() << " MMS_GetControlStatus failed: " << (int)ret << ", node: " << (int)3 << std::endl;
            }

            //DelayMilisecondImpl(500);
            //QCoreApplication::processEvents();
        }

        QCoreApplication::processEvents();  // Wait all events to be processed, especially serial port events.
        serialPort.close();
    }

    std::cout << "Program terminated.";
    QTimer::singleShot(0, &a, SLOT(quit()));    // Notify to quit
    return a.exec();
}
