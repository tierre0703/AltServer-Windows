#include "SignManager.h"

#include <windows.h>
#include <windowsx.h>
#include <strsafe.h>
#include <stdlib.h>
#include <string.h>
#include <tchar.h>

#include "AppleAPI.hpp"
#include "Signer.hpp"
#include <cpprest/http_client.h>
#include <cpprest/filestream.h>
#include <filesystem>
#include <WS2tcpip.h>
#include <ShlObj_core.h>
#include "InstallError.h"
#include "ServerError.h"

#include <fstream>
#include <iterator>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <codecvt>

#include "DeviceManager.h"
#include "Archiver.hpp"

#include "AnisetteDataManager.h"



#include <plist/plist.h>


#define odslog(msg) { std::stringstream ss; ss << msg << std::endl; OutputDebugStringA(ss.str().c_str()); }

using namespace utility;                    // Common utilities like string conversions
using namespace web;                        // Common features like URIs.
using namespace web::http;                  // Common HTTP functionality
using namespace web::http::client;          // HTTP client features
using namespace concurrency::streams;       // Asynchronous streams

namespace fs = std::filesystem;

std::string _verificationCode;

extern std::string temporary_directory();
extern std::string make_uuid();
extern std::vector<unsigned char> readFile(const char* filename);

extern std::string StringFromWideString(std::wstring wideString);
extern std::wstring WideStringFromString(std::string string);




std::string make_uuid()
{
    GUID guid;
    CoCreateGuid(&guid);

    std::ostringstream os;
    os << std::hex << std::setw(8) << std::setfill('0') << guid.Data1;
    os << '-';
    os << std::hex << std::setw(4) << std::setfill('0') << guid.Data2;
    os << '-';
    os << std::hex << std::setw(4) << std::setfill('0') << guid.Data3;
    os << '-';
    os << std::hex << std::setw(2) << std::setfill('0') << static_cast<short>(guid.Data4[0]);
    os << std::hex << std::setw(2) << std::setfill('0') << static_cast<short>(guid.Data4[1]);
    os << '-';
    os << std::hex << std::setw(2) << std::setfill('0') << static_cast<short>(guid.Data4[2]);
    os << std::hex << std::setw(2) << std::setfill('0') << static_cast<short>(guid.Data4[3]);
    os << std::hex << std::setw(2) << std::setfill('0') << static_cast<short>(guid.Data4[4]);
    os << std::hex << std::setw(2) << std::setfill('0') << static_cast<short>(guid.Data4[5]);
    os << std::hex << std::setw(2) << std::setfill('0') << static_cast<short>(guid.Data4[6]);
    os << std::hex << std::setw(2) << std::setfill('0') << static_cast<short>(guid.Data4[7]);

    std::string s(os.str());
    return s;
}

std::string temporary_directory()
{
    wchar_t rawTempDirectory[1024];

    int length = GetTempPath(1024, rawTempDirectory);

    std::wstring wideString(rawTempDirectory);

    std::wstring_convert<std::codecvt_utf8<wchar_t>> conv1;
    std::string tempDirectory = conv1.to_bytes(wideString);

    return tempDirectory;
}

std::vector<unsigned char> readFile(const char* filename)
{
    // open the file:
    std::ifstream file(filename, std::ios::binary);

    // Stop eating new lines in binary mode!!!
    file.unsetf(std::ios::skipws);

    // get its size:
    std::streampos fileSize;

    file.seekg(0, std::ios::end);
    fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    // reserve capacity
    std::vector<unsigned char> vec;
    vec.reserve(fileSize);

    // read the data:
    vec.insert(vec.begin(),
        std::istream_iterator<unsigned char>(file),
        std::istream_iterator<unsigned char>());

    return vec;
}




std::string StringFromWideString(std::wstring wideString)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

    std::string string = converter.to_bytes(wideString);
    return string;
}

std::wstring WideStringFromString(std::string string)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;

    std::wstring wideString = converter.from_bytes(string);
    return wideString;
}


SignManager* SignManager::_instance = nullptr;

SignManager* SignManager::instance()
{
    if (_instance == 0)
    {
        _instance = new SignManager();
    }

    return _instance;
}

SignManager::SignManager()
{
}

SignManager::~SignManager()
{
}


std::string SignManager::serverID() const
{
    //auto serverID = GetRegistryStringValue(SERVER_ID_KEY);
    //return serverID;
    return _serverID;
}

void SignManager::setServerID(std::string serverID)
{
    //SetRegistryStringValue(SERVER_ID_KEY, serverID);
    _serverID = serverID;
}

pplx::task<void> SignManager::_SignProc(std::shared_ptr<Device> installDevice, std::string appleID, std::string password)
{
    fs::path destinationDirectoryPath(temporary_directory());
    destinationDirectoryPath.append(make_uuid());

    auto account = std::make_shared<Account>();
    auto app = std::make_shared<Application>();
    auto team = std::make_shared<Team>();
    auto device = std::make_shared<Device>();
    auto appID = std::make_shared<AppID>();
    auto certificate = std::make_shared<Certificate>();
    auto profile = std::make_shared<ProvisioningProfile>();

    auto session = std::make_shared<AppleAPISession>();

    return pplx::create_task([=]() {
        auto anisetteData = AnisetteDataManager::instance()->FetchAnisetteData();
        return this->Authenticate(appleID, password, anisetteData);
        })
        .then([=](std::pair<std::shared_ptr<Account>, std::shared_ptr<AppleAPISession>> pair)
            {
                *account = *(pair.first);
                *session = *(pair.second);

                odslog("Fetching team...");

                return this->FetchTeam(account, session);
            })
            .then([=](std::shared_ptr<Team> tempTeam)
                {
                    odslog("Registering device...");

                    *team = *tempTeam;
                    return this->RegisterDevice(installDevice, team, session);
                })
                .then([=](std::shared_ptr<Device> tempDevice)
                    {
                        odslog("Fetching certificate...");

                        *device = *tempDevice;
                        return this->FetchCertificate(team, session);
                    })
                    .then([=](std::shared_ptr<Certificate> tempCertificate)
                        {
                            *certificate = *tempCertificate;

                            std::stringstream ssTitle;
                            ssTitle << "Installing AltStore to " << installDevice->name() << "...";

                            std::stringstream ssMessage;
                            ssMessage << "This may take a few seconds.";

                            //this->ShowNotification(ssTitle.str(), ssMessage.str());

                            odslog("Downloading app...");

                            return this->DownloadApp();
                        })
                        .then([=](fs::path downloadedAppPath)
                            {
                                odslog("Downloaded app!");

                                fs::create_directory(destinationDirectoryPath);

                                auto appBundlePath = UnzipAppBundle(downloadedAppPath.string(), destinationDirectoryPath.string());

                                try
                                {
                                    fs::remove(downloadedAppPath);
                                }
                                catch (std::exception& e)
                                {
                                    odslog("Failed to remove downloaded .ipa." << e.what());
                                }

                                auto app = std::make_shared<Application>(appBundlePath);
                                return app;
                            })
                            .then([=](std::shared_ptr<Application> tempApp)
                                {
                                    *app = *tempApp;
                                    return this->RegisterAppID(app->name(), app->bundleIdentifier(), team, session);
                                })
                                .then([=](std::shared_ptr<AppID> tempAppID)
                                    {
                                        *appID = *tempAppID;
                                        return this->FetchProvisioningProfile(appID, team, session);
                                    })
                                    .then([=](std::shared_ptr<ProvisioningProfile> tempProfile)
                                        {
                                            *profile = *tempProfile;
                                            return this->InstallApp(app, device, team, appID, certificate, profile);
                                        })
                                        .then([=](pplx::task<void> task)
                                            {
                                                if (fs::exists(destinationDirectoryPath))
                                                {
                                                    fs::remove_all(destinationDirectoryPath);
                                                }

                                                try
                                                {
                                                    task.get();
                                                }
                                                catch (LocalizedError& error)
                                                {
                                                    if (error.code() == -22421)
                                                    {
                                                        // Don't know what API call returns this error code, so assume any LocalizedError with -22421 error code
                                                        // means invalid anisette data, then throw the correct APIError.
                                                        throw APIError(APIErrorCode::InvalidAnisetteData);
                                                    }
                                                    else
                                                    {
                                                        throw;
                                                    }
                                                }
                                            });
}



pplx::task<fs::path> SignManager::DownloadApp()
{
    fs::path temporaryPath(temporary_directory());
    temporaryPath.append(make_uuid());

    auto outputFile = std::make_shared<ostream>();

    // Open stream to output file.
    auto task = fstream::open_ostream(WideStringFromString(temporaryPath.string()))
        .then([=](ostream file)
            {
                *outputFile = file;

                uri_builder builder(L"https://f000.backblazeb2.com/file/altstore/altstore.ipa");

                http_client client(builder.to_uri());
                return client.request(methods::GET);
            })
        .then([=](http_response response)
            {
                printf("Received download response status code:%u\n", response.status_code());

                // Write response body into the file.
                return response.body().read_to_end(outputFile->streambuf());
            })
                .then([=](size_t)
                    {
                        outputFile->close();
                        return temporaryPath;
                    });

            return task;
}


pplx::task<void> SignManager::InstallApp(std::shared_ptr<Application> app,
    std::shared_ptr<Device> device,
    std::shared_ptr<Team> team,
    std::shared_ptr<AppID> appID,
    std::shared_ptr<Certificate> certificate,
    std::shared_ptr<ProvisioningProfile> profile)
{
    return pplx::task<void>([=]() {
        fs::path infoPlistPath(app->path());
        infoPlistPath.append("Info.plist");

        auto data = readFile(infoPlistPath.string().c_str());

        plist_t plist = nullptr;
        plist_from_memory((const char*)data.data(), (int)data.size(), &plist);
        if (plist == nullptr)
        {
            throw InstallError(InstallErrorCode::MissingInfoPlist);
        }

        plist_dict_set_item(plist, "CFBundleIdentifier", plist_new_string(profile->bundleIdentifier().c_str()));
        plist_dict_set_item(plist, "ALTDeviceID", plist_new_string(device->identifier().c_str()));

        auto serverID = this->serverID();
        plist_dict_set_item(plist, "ALTServerID", plist_new_string(serverID.c_str()));

        plist_dict_set_item(plist, "ALTCertificateID", plist_new_string(certificate->serialNumber().c_str()));

        char* plistXML = nullptr;
        uint32_t length = 0;
        plist_to_xml(plist, &plistXML, &length);

        std::ofstream fout(infoPlistPath.string(), std::ios::out | std::ios::binary);
        fout.write(plistXML, length);
        fout.close();

        auto machineIdentifier = certificate->machineIdentifier();
        if (machineIdentifier.has_value())
        {
            auto encryptedData = certificate->encryptedP12Data(*machineIdentifier);
            if (encryptedData.has_value())
            {
                // Embed encrypted certificate in app bundle.
                fs::path certificatePath(app->path());
                certificatePath.append("ALTCertificate.p12");

                std::ofstream fout(certificatePath.string(), std::ios::out | std::ios::binary);
                fout.write((const char*)encryptedData->data(), length);
                fout.close();
            }
        }

        Signer signer(team, certificate);
        signer.SignApp(app->path(), { profile });

        return DeviceManager::instance()->InstallApp(app->path(), device->identifier(), [](double progress) {
            odslog("AltStore Installation Progress: " << progress);
            });
        });
}

bool SignManager::CheckiCloudDependencies()
{
    wchar_t programFilesCommonDirectory[MAX_PATH];

    GetCurrentDirectoryW(MAX_PATH, programFilesCommonDirectory);

    fs::path deviceDriverDirectoryPath(programFilesCommonDirectory);
    //deviceDriverDirectoryPath.append("Apple").append("Internet Services");
    deviceDriverDirectoryPath.append("iCloud");

    fs::path aosKitPath(deviceDriverDirectoryPath);
    aosKitPath.append("AOSKit.dll");

    if (!fs::exists(aosKitPath))
    {
        return false;
    }

    return true;
}


pplx::task<std::pair<std::shared_ptr<Account>, std::shared_ptr<AppleAPISession>>> SignManager::Authenticate(std::string appleID, std::string password, std::shared_ptr<AnisetteData> anisetteData)
{
    auto verificationHandler = [=](void)->pplx::task<std::optional<std::string>> {
        return pplx::create_task([=]() -> std::optional<std::string> {
            /*
            int result = DialogBox(NULL, MAKEINTRESOURCE(ID_TWOFACTOR), NULL, TwoFactorDlgProc);
            if (result == IDCANCEL)
            {
                return std::nullopt;
            }
            */
            auto verificationCode = std::make_optional<std::string>(_verificationCode);
            _verificationCode = "";

            return verificationCode;
            });
    };

    return pplx::create_task([=]() {
        if (anisetteData == NULL)
        {
            throw ServerError(ServerErrorCode::InvalidAnisetteData);
        }

        return AppleAPI::getInstance()->Authenticate(appleID, password, anisetteData, verificationHandler);
        });
}



pplx::task<std::shared_ptr<Team>> SignManager::FetchTeam(std::shared_ptr<Account> account, std::shared_ptr<AppleAPISession> session)
{
    auto task = AppleAPI::getInstance()->FetchTeams(account, session)
        .then([](std::vector<std::shared_ptr<Team>> teams) {

        for (auto& team : teams)
        {
            if (team->type() == Team::Type::Free)
            {
                return team;
            }
        }

        for (auto& team : teams)
        {
            if (team->type() == Team::Type::Individual)
            {
                return team;
            }
        }

        for (auto& team : teams)
        {
            return team;
        }

        throw InstallError(InstallErrorCode::NoTeam);
            });

    return task;
}

pplx::task<std::shared_ptr<Certificate>> SignManager::FetchCertificate(std::shared_ptr<Team> team, std::shared_ptr<AppleAPISession> session)
{
    auto task = AppleAPI::getInstance()->FetchCertificates(team, session)
        .then([this, team, session](std::vector<std::shared_ptr<Certificate>> certificates)
            {
                std::shared_ptr<Certificate> preferredCertificate = nullptr;

                for (auto& certificate : certificates)
                {
                    if (!certificate->machineName().has_value())
                    {
                        continue;
                    }

                    std::string prefix("AltStore");

                    if (certificate->machineName()->size() < prefix.size())
                    {
                        // Machine name doesn't begin with "AltStore", so ignore.
                        continue;
                    }
                    else
                    {
                        auto result = std::mismatch(prefix.begin(), prefix.end(), certificate->machineName()->begin());
                        if (result.first != prefix.end())
                        {
                            // Machine name doesn't begin with "AltStore", so ignore.
                            continue;
                        }
                    }

                    preferredCertificate = certificate;

                    // Machine name starts with AltStore.

                    auto alertResult = MessageBox(NULL,
                        L"Apps installed with AltStore on your other devices will stop working. Are you sure you want to continue?",
                        L"AltStore already installed on another device.",
                        MB_OKCANCEL);

                    if (alertResult == IDCANCEL)
                    {
                        throw InstallError(InstallErrorCode::Cancelled);
                    }

                    break;
                }

                if (certificates.size() != 0)
                {
                    auto certificate = (preferredCertificate != nullptr) ? preferredCertificate : certificates[0];
                    return AppleAPI::getInstance()->RevokeCertificate(certificate, team, session).then([this, team, session](bool success)
                        {
                            return this->FetchCertificate(team, session);
                        });
                }
                else
                {
                    std::string machineName = "AltStore";

                    return AppleAPI::getInstance()->AddCertificate(machineName, team, session).then([team, session](std::shared_ptr<Certificate> addedCertificate)
                        {
                            auto privateKey = addedCertificate->privateKey();
                            if (privateKey == std::nullopt)
                            {
                                throw InstallError(InstallErrorCode::MissingPrivateKey);
                            }

                            return AppleAPI::getInstance()->FetchCertificates(team, session)
                                .then([privateKey, addedCertificate](std::vector<std::shared_ptr<Certificate>> certificates)
                                    {
                                        std::shared_ptr<Certificate> certificate = nullptr;

                                        for (auto tempCertificate : certificates)
                                        {
                                            if (tempCertificate->serialNumber() == addedCertificate->serialNumber())
                                            {
                                                certificate = tempCertificate;
                                                break;
                                            }
                                        }

                                        if (certificate == nullptr)
                                        {
                                            throw InstallError(InstallErrorCode::MissingCertificate);
                                        }

                                        certificate->setPrivateKey(privateKey);
                                        return certificate;
                                    });
                        });
                }
            });

    return task;
}


pplx::task<std::shared_ptr<AppID>> SignManager::RegisterAppID(std::string appName, std::string identifier, std::shared_ptr<Team> team, std::shared_ptr<AppleAPISession> session)
{
    std::stringstream ss;
    ss << "com." << team->identifier() << "." << identifier;

    auto bundleID = ss.str();

    auto task = AppleAPI::getInstance()->FetchAppIDs(team, session)
        .then([bundleID, appName, identifier, team, session](std::vector<std::shared_ptr<AppID>> appIDs)
            {
                std::shared_ptr<AppID> appID = nullptr;

                for (auto tempAppID : appIDs)
                {
                    if (tempAppID->bundleIdentifier() == bundleID)
                    {
                        appID = tempAppID;
                        break;
                    }
                }

                if (appID != nullptr)
                {
                    return pplx::task<std::shared_ptr<AppID>>([appID]()
                        {
                            return appID;
                        });
                }
                else
                {
                    return AppleAPI::getInstance()->AddAppID(appName, bundleID, team, session);
                }
            });

    return task;
}


pplx::task<std::shared_ptr<Device>> SignManager::RegisterDevice(std::shared_ptr<Device> device, std::shared_ptr<Team> team, std::shared_ptr<AppleAPISession> session)
{
    auto task = AppleAPI::getInstance()->FetchDevices(team, session)
        .then([device, team, session](std::vector<std::shared_ptr<Device>> devices)
            {
                std::shared_ptr<Device> matchingDevice = nullptr;

                for (auto tempDevice : devices)
                {
                    if (tempDevice->identifier() == device->identifier())
                    {
                        matchingDevice = tempDevice;
                        break;
                    }
                }

                if (matchingDevice != nullptr)
                {
                    return pplx::task<std::shared_ptr<Device>>([matchingDevice]()
                        {
                            return matchingDevice;
                        });
                }
                else
                {
                    return AppleAPI::getInstance()->RegisterDevice(device->name(), device->identifier(), team, session);
                }
            });

    return task;
}


pplx::task<std::shared_ptr<ProvisioningProfile>> SignManager::FetchProvisioningProfile(std::shared_ptr<AppID> appID, std::shared_ptr<Team> team, std::shared_ptr<AppleAPISession> session)
{
    return AppleAPI::getInstance()->FetchProvisioningProfile(appID, team, session);
}


bool SignManager::reprovisionedDevice() const
{
    //auto reprovisionedDevice = GetRegistryBoolValue(REPROVISIONED_DEVICE_KEY);
    //return reprovisionedDevice;
    return _reprovisionedDevice;
}

void SignManager::setReprovisionedDevice(bool reprovisionedDevice)
{
    //SetRegistryBoolValue(REPROVISIONED_DEVICE_KEY, reprovisionedDevice);
    _reprovisionedDevice = reprovisionedDevice;
}