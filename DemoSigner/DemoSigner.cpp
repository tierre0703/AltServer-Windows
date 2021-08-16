// DemoSigner.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include "SignManager.h"
#include "DeviceManager.hpp"
#include "AnisetteDataManager.h"
#include "AppleAPI.hpp"
#include <WinSock2.h>
#include "AnisetteData.h"
std::string strIpaName;
std::string strAppleId;
std::string strApplePwd;
std::string strDeviceId;

bool ParseArguments(int argc, char* args[])
{
    if (argc < 5)
    {
        std::cout << "Argument missing\n";
        std::cout << "Usage: DemoSigner.exe IPANAME APPLEID APPLEPWD DEVICEUDID\n";
        return false;
    }

    strIpaName = args[1];
    strAppleId = args[2];
    strApplePwd = args[3];
    strDeviceId = args[4];

    return true;
}

int main(int argc, char* args[])
{
    //std::cout << "Hello World!\n";
    std::cout << "Apple's IPA Demo Signer Application version 1.0.0\n";
    std::cout << "This Application is for sign and install iPhone/iPad application with free AppleID\n\n";
    std::cout << "Prerequisites: please install iTunes and iCloud application from Apple's store\n";
    std::cout << "iTunes URL: https://www.apple.com/itunes/download/win32\n";
    std::cout << "iCloud URL: https://secure-appldnld.apple.com/windows/061-91601-20200323-974a39d0-41fc-4761-b571-318b7d9205ed/iCloudSetup.exe\n";
    std::cout << "\n\n";
    std::cout << "Usage: DemoSigner.exe IPANAME APPLEID APPLEPWD DEVICEUDID\n\n";
    std::cout << "IPANAME       full path and name of target .ipa file\n";
    std::cout << "APPLEID       valid apple id to sign the target .ipa\n";
    std::cout << "APPLEPWD      valid apple password to sign the target .ipa\n";
    std::cout << "DEVICEUDID    target device's UDID which connected to the current machine\n\n";


    if (ParseArguments(argc, args)) {

        //strIpaName = "C:\\Users\\prince\\Downloads\\plenixclash-v13.369.29.ipa";
        //strAppleId = "khj20130703@hotmail.com";
        //strApplePwd = "Jinxue11!!";
        //strDeviceId = "BCE88EA1C399F444A56931F2110E8DF3EBCA2FCB";
        std::cout << "IPA PATH: " << strIpaName << "\n";
        std::cout << "Apple ID: " << strAppleId << "\n";
        std::cout << "Apple Pwd: " << "********" << "\n";
        std::cout << "Device ID: " << strDeviceId << "\n\n";


        auto devices = DeviceManager::instance()->connectedDevices();

        bool deviceFound = false;
        std::shared_ptr<Device> targetDevice = NULL;

        if (devices.size() == 0) {
            std::cout << "No Devices are Connected\n";
            return 0;
        }

        for (auto& c : strDeviceId)
        {
            c = tolower(c);
        }

        for (int i = 0; i < devices.size(); i++)
        {
            auto _dev = devices[i];
            if (_dev->identifier() == strDeviceId)
            {
                // Duplicate.
                deviceFound = true;
                targetDevice = _dev;
                break;
            }
        }

        if (deviceFound == false) {
            std::cout << "Current Device " << strDeviceId.c_str() << " not connected. Please connect device and retry again\n";
            return 0;
        }

        targetDevice = devices[0];

        auto task_status = SignManager::instance()->InstallApplication(strIpaName, targetDevice, strAppleId, strApplePwd).wait();
    }


}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file