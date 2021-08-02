// DemoSigner.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>

int parseArgs(int argc, char* argv[]) {
    for (int i = 0; i < argc; i++) {
        
    }
}

int main(int argc, char* argv[])
{
    //std::cout << "Hello World!\n";
    std::cout << "Demo IPA signer tool\n";
    std::cout << "Usage: DemoSigner appleId password ipa_filename device_udid\n";
    std::cout << "\n\n\n";

    std::string strAppleId;
    std::string strPassword;
    std::string strIPAFileName;
    std::string strDeviceUDID;


    if (argc < 4) {
        return 0;
    }

    strAppleId = argv[0];
    strPassword = argv[1];
    strIPAFileName = argv[2];
    strDeviceUDID = argv[3];






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
