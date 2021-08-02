// DemoSigner.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

#include "SignManager.h"
#include "DeviceManager.hpp"

std::string strIpaName;
std::string strAppleId;
std::string strApplePwd;
std::string strDeviceId;

bool ParseArguments(int argc, char* args[])
{
    if (argc < 4) 
    {
        std::cout << "Argument missing\n";
        std::cout << "Usage: DemoSigner.exe IPANAME APPLEID APPLEPWD DEVICEUDID\n";
        return false;
    }

    strIpaName = args[0];
    strAppleId = args[1];
    strApplePwd = args[2];
    strDeviceId = args[3];

    return true;
}



int main(int argc, char* args[])
{
    //std::cout << "Hello World!\n";
    std::cout << "IPA demo signer\n";
    std::cout << "Usage: DemoSigner.exe IPANAME APPLEID APPLEPWD DEVICEUDID\n";


    if (ParseArguments(argc, args)) {
        auto devices = DeviceManager::instance()->connectedDevices();

        bool deviceFound = false;
        std::shared_ptr<Device> targetDevice = NULL;
        
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
            std::cout << "Current Device not connected\n";
            return 0;
        }

        auto task = SignManager::instance()->InstallApplication(strIpaName, targetDevice, strAppleId, strApplePwd);
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
