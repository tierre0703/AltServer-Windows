//
//  AltServerApp.cpp
//  AltServer-Windows
//
//  Created by Riley Testut on 8/30/19.
//  Copyright (c) 2019 Riley Testut. All rights reserved.
//

#include "SignManager.h"
#include <windows.h>
#include <windowsx.h>
#include <strsafe.h>

#include "AppleAPI.hpp"
#include "InstallError.hpp"
#include "Signer.hpp"
#include "DeviceManager.hpp"
#include "Archiver.hpp"
#include "ServerError.hpp"

#include "Semaphore.h"

#include "AnisetteDataManager.h"

#include <cpprest/http_client.h>
#include <cpprest/filestream.h>

#include <iostream>
#include <fstream>
#include <filesystem>

#include <plist/plist.h>

#include <WS2tcpip.h>
#include <ShlObj_core.h>

#pragma comment( lib, "gdiplus.lib" ) 
#include <gdiplus.h> 
#include <strsafe.h>

#include <codecvt>
//#include <winsparkle.h>

#define odslog(msg) { std::stringstream ss; ss << msg << std::endl; OutputDebugStringA(ss.str().c_str()); }

using namespace utility;                    // Common utilities like string conversions
using namespace web;                        // Common features like URIs.
using namespace web::http;                  // Common HTTP functionality
using namespace web::http::client;          // HTTP client features
using namespace concurrency::streams;       // Asynchronous streams

namespace fs = std::filesystem;

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


std::string _verificationCode;

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


void SignManager::CheckForUpdates()
{
	//win_sparkle_check_update_with_ui();
}

pplx::task<void> SignManager::InstallApplication(std::string filepath, std::shared_ptr<Device> installDevice, std::string appleID, std::string password)
{
	return this->_InstallApplication(filepath, installDevice, appleID, password)
		.then([=](pplx::task<void> task) -> pplx::task<void> {
		try
		{
			task.get();
			return pplx::create_task([]() {});
		}
		catch (APIError& error)
		{
			if ((APIErrorCode)error.code() == APIErrorCode::InvalidAnisetteData)
			{
				// Our attempt to re-provision the device as a Mac failed, so reset provisioning and try one more time.
				// This appears to happen when iCloud is running simultaneously, and just happens to provision device at same time as AltServer.
				AnisetteDataManager::instance()->ResetProvisioning();

				this->ShowNotification("Registering PC with Apple...", "This may take a few seconds.");

				// Provisioning device can fail if attempted too soon after previous attempt.
				// As a hack around this, we wait a bit before trying again.
				// 10-11 seconds appears to be too short, so wait for 12 seconds instead.
				Sleep(12000);

				return this->_InstallApplication(filepath, installDevice, appleID, password);
			}
			else
			{
				throw;
			}
		}
			})
		.then([=](pplx::task<void> task) -> void {
				try
				{
					task.get();

					std::stringstream ss;
					ss << filepath.c_str() << " was successfully installed on " << installDevice->name() << "." << "\n";

					this->ShowNotification("Installation Succeeded", ss.str());
				}
				catch (InstallError& error)
				{
					if ((InstallErrorCode)error.code() == InstallErrorCode::Cancelled)
					{
						// Ignore
					}
					else
					{
						this->ShowAlert("Installation Failed", error.localizedDescription());
						throw;
					}
				}
				catch (APIError& error)
				{
					if ((APIErrorCode)error.code() == APIErrorCode::InvalidAnisetteData)
					{
						AnisetteDataManager::instance()->ResetProvisioning();
					}

					this->ShowAlert("Installation Failed", error.localizedDescription());
					throw;
				}
				catch (AnisetteError& error)
				{
					this->HandleAnisetteError(error);
					throw;
				}
				catch (Error& error)
				{
					this->ShowAlert("Installation Failed", error.localizedDescription());
					throw;
				}
				catch (std::exception& exception)
				{
					odslog("Exception:" << exception.what());

					this->ShowAlert("Installation Failed", exception.what());
					throw;
				}
			});
}

pplx::task<void> SignManager::_InstallApplication(std::string filepath, std::shared_ptr<Device> installDevice, std::string appleID, std::string password)
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
				std::cout << "\nFetching team from Apple ...\n";
				return this->FetchTeam(account, session);
			})
			.then([=](std::shared_ptr<Team> tempTeam)
				{
					std::cout << "Fetching team from Apple Done\n";
					odslog("Registering device...");
					std::cout << "Registering device to Apple...\n";

					*team = *tempTeam;
					return this->RegisterDevice(installDevice, team, session);
				})
				.then([=](std::shared_ptr<Device> tempDevice)
					{
						std::cout << "Registering device to Apple Done\n";
						odslog("Fetching certificate...");
						std::cout << "Fetching certificate...\n";

						*device = *tempDevice;
						return this->FetchCertificate(team, session);
					})
					.then([=](std::shared_ptr<Certificate> tempCertificate)
						{
							*certificate = *tempCertificate;
							std::cout << "Fetching certificate Done\n";

							std::stringstream ssTitle;
							ssTitle << "Installing "<< filepath <<" to " << installDevice->name() << "...";

							std::stringstream ssMessage;
							ssMessage << "This may take a few seconds.";

							this->ShowNotification(ssTitle.str(), ssMessage.str());

							std::cout <<"Writing app to Temp Directory...\n";

							fs::path downloadedAppPath(temporary_directory());
							downloadedAppPath.append(make_uuid());
							fs::copy(filepath, downloadedAppPath.string());
							//return temporaryPath;

							//return this->DownloadApp();
							//return this->WriteFileToWorkDirectory(filepath);
						//})
						//.then([=](fs::path downloadedAppPath)
						//	{
								std::cout << downloadedAppPath.string()  <<"\nWriting app to Temp Directory Done!\n";

								fs::create_directory(destinationDirectoryPath);

								auto appBundlePath = UnzipAppBundle(downloadedAppPath.string(), destinationDirectoryPath.string());
								std::cout << "UnzipAppBundle app for signing!\n";
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
									return this->PrepareAllProvisioningProfiles(app, team, session);
								})
								.then([=](std::map<std::string, std::shared_ptr<ProvisioningProfile>> profiles)
									{
										return this->InstallApp(app, device, team, certificate, profiles);
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
												else if (error.code() == -29004)
												{
													// Same with -29004, "Environment Mismatch"
													throw APIError(APIErrorCode::InvalidAnisetteData);
												}
												else
												{
													throw;
												}
											}
										});
}

pplx::task<fs::path> SignManager::WriteFileToWorkDirectory(std::string filepath)
{

	

	fs::path temporaryPath(temporary_directory());
	temporaryPath.append(make_uuid());

	auto inputFile = std::make_shared<istream>();
	auto outputFile = std::make_shared<ostream>();

	*inputFile = fstream::open_istream(WideStringFromString(filepath)).get();
	auto task = fstream::open_ostream(WideStringFromString(temporaryPath.string()))
		.then([=](ostream file) {
		*outputFile = file;
		return inputFile->read_to_end(outputFile->streambuf());
			}).then([=](size_t)
				{
					outputFile->close();
					inputFile->close();
					return temporaryPath;
				});
			return task;
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

				uri_builder builder(L"https://cdn.altstore.io/file/altstore/altstore.ipa");

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

pplx::task<std::pair<std::shared_ptr<Account>, std::shared_ptr<AppleAPISession>>> SignManager::Authenticate(std::string appleID, std::string password, std::shared_ptr<AnisetteData> anisetteData)
{
	auto verificationHandler = [=](void)->pplx::task<std::optional<std::string>> {
		return pplx::create_task([=]() -> std::optional<std::string> {

			std::cout << "Please input TwoFactor Verfication Code:\n";
			std::string verificationCode;
			std::cin >> verificationCode;
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

					//auto alertResult = MessageBox(NULL,
					//	L"Apps installed with AltStore on your other devices will stop working. Are you sure you want to continue?",
					//	L"AltStore already installed on another device.",
					//	MB_OKCANCEL);

					//if (alertResult == IDCANCEL)
					//{
					//	throw InstallError(InstallErrorCode::Cancelled);
					//}

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
					std::string machineName = "DemoSigner";

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

pplx::task<std::map<std::string, std::shared_ptr<ProvisioningProfile>>> SignManager::PrepareAllProvisioningProfiles(
	std::shared_ptr<Application> application,
	std::shared_ptr<Team> team,
	std::shared_ptr<AppleAPISession> session)
{
	return this->PrepareProvisioningProfile(application, team, session)
		.then([=](std::shared_ptr<ProvisioningProfile> profile) {
		std::map<std::string, std::shared_ptr<ProvisioningProfile>> profiles = { {application->bundleIdentifier(), profile} };

		// Initialize with negative value so we wait immediately.
		int count = -(int)(application->appExtensions().size()) + 1;
		Semaphore semaphore(count);

		for (auto appExtension : application->appExtensions())
		{
			this->PrepareProvisioningProfile(appExtension, team, session)
				.then([appExtension, &profiles, &semaphore](std::shared_ptr<ProvisioningProfile> profile) {
				profiles[appExtension->bundleIdentifier()] = profile;
				semaphore.notify();
					});
		}

		semaphore.wait();

		return profiles;
			});
}

pplx::task<std::shared_ptr<ProvisioningProfile>> SignManager::PrepareProvisioningProfile(
	std::shared_ptr<Application> app,
	std::shared_ptr<Team> team,
	std::shared_ptr<AppleAPISession> session)
{
	auto appID = std::make_shared<AppID>();

	return this->RegisterAppID(app->name(), app->bundleIdentifier(), team, session)
		.then([=](std::shared_ptr<AppID> appID)
			{
				return this->UpdateAppIDFeatures(appID, app, team, session);
			})
		.then([=](std::shared_ptr<AppID> tempAppID)
			{
				*appID = *tempAppID;
				return this->UpdateAppIDAppGroups(appID, app, team, session);
			})
				.then([=](bool success)
					{
						return this->FetchProvisioningProfile(appID, team, session);
					})
				.then([=](std::shared_ptr<ProvisioningProfile> profile)
					{
						return profile;
					});
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

pplx::task<std::shared_ptr<AppID>> SignManager::UpdateAppIDFeatures(std::shared_ptr<AppID> appID, std::shared_ptr<Application> app, std::shared_ptr<Team> team, std::shared_ptr<AppleAPISession> session)
{
	//TODO: Add support for additional features besides app groups.

	std::map<std::string, plist_t> altstoreFeatures = appID->features();
	altstoreFeatures[AppIDFeatureAppGroups] = plist_new_bool(true);

	//TODO: Only update features if needed.

	std::shared_ptr<AppID> copiedAppID = std::make_shared<AppID>(*appID);
	copiedAppID->setFeatures(altstoreFeatures);

	return AppleAPI::getInstance()->UpdateAppID(copiedAppID, team, session);
}

pplx::task<bool> SignManager::UpdateAppIDAppGroups(std::shared_ptr<AppID> appID, std::shared_ptr<Application> app, std::shared_ptr<Team> team, std::shared_ptr<AppleAPISession> session)
{
	//TODO: Read app groups from app entitlements.
	//TODO: Add locks to prevent race conditions with multiple extensions.

	std::string applicationGroup = "group.com.jin.demosigner";
	std::string adjustedGroupIdentifier = applicationGroup + "." + team->identifier();

	return AppleAPI::getInstance()->FetchAppGroups(team, session)
		.then([=](std::vector<std::shared_ptr<AppGroup>> groups) {
		for (auto group : groups)
		{
			if (group->groupIdentifier() == adjustedGroupIdentifier)
			{
				return pplx::create_task([group]() {
					return group;
					});
			}
		}

		std::string name = "DemoSigner " + applicationGroup;
		std::replace(name.begin(), name.end(), '.', ' ');

		return AppleAPI::getInstance()->AddAppGroup(name, adjustedGroupIdentifier, team, session);
			})
		.then([=](std::shared_ptr<AppGroup> group) {
				return AppleAPI::getInstance()->AssignAppIDToGroups(appID, { group }, team, session);
			});
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

pplx::task<void> SignManager::InstallApp(std::shared_ptr<Application> app,
	std::shared_ptr<Device> device,
	std::shared_ptr<Team> team,
	std::shared_ptr<Certificate> certificate,
	std::map<std::string, std::shared_ptr<ProvisioningProfile>> profilesByBundleID)
{
	auto prepareInfoPlist = [profilesByBundleID](std::shared_ptr<Application> app, plist_t additionalValues) {
		auto profile = profilesByBundleID.at(app->bundleIdentifier());

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
		plist_dict_set_item(plist, "ALTBundleIdentifier", plist_new_string(app->bundleIdentifier().c_str()));

		if (additionalValues != NULL)
		{
			plist_dict_merge(&plist, additionalValues);
		}

		plist_t entitlements = profile->entitlements();
		if (entitlements != nullptr)
		{
			plist_t appGroups = plist_copy(plist_dict_get_item(entitlements, "com.apple.security.application-groups"));
			plist_dict_set_item(plist, "ALTAppGroups", appGroups);
		}

		char* plistXML = nullptr;
		uint32_t length = 0;
		plist_to_xml(plist, &plistXML, &length);

		std::ofstream fout(infoPlistPath.string(), std::ios::out | std::ios::binary);
		fout.write(plistXML, length);
		fout.close();
	};

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

		plist_t additionalValues = plist_new_dict();
		plist_dict_set_item(additionalValues, "ALTDeviceID", plist_new_string(device->identifier().c_str()));

		auto serverID = this->serverID();
		plist_dict_set_item(additionalValues, "ALTServerID", plist_new_string(serverID.c_str()));

		std::string openAppURLScheme = "altstore-" + app->bundleIdentifier();

		plist_t allURLSchemes = plist_copy(plist_dict_get_item(plist, "CFBundleURLTypes"));
		if (allURLSchemes == nullptr)
		{
			allURLSchemes = plist_new_array();
		}

		plist_t altstoreURLScheme = plist_new_dict();
		plist_dict_set_item(altstoreURLScheme, "CFBundleTypeRole", plist_new_string("Editor"));
		plist_dict_set_item(altstoreURLScheme, "CFBundleURLName", plist_new_string(app->bundleIdentifier().c_str()));

		plist_t schemesNode = plist_new_array();
		plist_array_append_item(schemesNode, plist_new_string(openAppURLScheme.c_str()));
		plist_dict_set_item(altstoreURLScheme, "CFBundleURLSchemes", schemesNode);

		plist_array_append_item(allURLSchemes, altstoreURLScheme);
		plist_dict_set_item(additionalValues, "CFBundleURLTypes", allURLSchemes);

		auto machineIdentifier = certificate->machineIdentifier();
		if (machineIdentifier.has_value())
		{
			auto encryptedData = certificate->encryptedP12Data(*machineIdentifier);
			if (encryptedData.has_value())
			{
				plist_dict_set_item(additionalValues, "ALTCertificateID", plist_new_string(certificate->serialNumber().c_str()));

				// Embed encrypted certificate in app bundle.
				fs::path certificatePath(app->path());
				certificatePath.append("ALTCertificate.p12");

				std::ofstream fout(certificatePath.string(), std::ios::out | std::ios::binary);
				fout.write((const char*)encryptedData->data(), encryptedData->size());
				fout.close();
			}
		}

		prepareInfoPlist(app, additionalValues);

		for (auto appExtension : app->appExtensions())
		{
			prepareInfoPlist(appExtension, NULL);
		}

		std::vector<std::shared_ptr<ProvisioningProfile>> profiles;
		std::set<std::string> profileIdentifiers;
		for (auto pair : profilesByBundleID)
		{
			profiles.push_back(pair.second);
			profileIdentifiers.insert(pair.second->bundleIdentifier());
		}

		Signer signer(team, certificate);
		
		signer.SignApp(app->path(), profiles);

		std::optional<std::set<std::string>> activeProfiles = std::nullopt;
		if (team->type() == Team::Type::Free)
		{
			activeProfiles = profileIdentifiers;
		}


		return DeviceManager::instance()->InstallApp(app->path(), device->identifier(), activeProfiles, [](double progress) {
			odslog("AltStore Installation Progress: " << progress);
			});
		});
}

void SignManager::ShowNotification(std::string title, std::string message)
{
	std::cout << title.c_str()<< ": " << message.c_str() << "\n";
}

void SignManager::ShowAlert(std::string title, std::string message)
{
	MessageBoxW(NULL, WideStringFromString(message).c_str(), WideStringFromString(title).c_str(), MB_OK);
}

bool SignManager::CheckDependencies()
{
	wchar_t* programFilesCommonDirectory;
	SHGetKnownFolderPath(FOLDERID_ProgramFilesCommon, 0, NULL, &programFilesCommonDirectory);

	fs::path deviceDriverDirectoryPath(programFilesCommonDirectory);
	deviceDriverDirectoryPath.append("Apple").append("Mobile Device Support");

	if (!fs::exists(deviceDriverDirectoryPath))
	{
		return false;
	}

	wchar_t* programFilesDirectory;
	SHGetKnownFolderPath(FOLDERID_ProgramFiles, 0, NULL, &programFilesDirectory);

	fs::path bonjourDirectoryPath(programFilesDirectory);
	bonjourDirectoryPath.append("Bonjour");

	if (!fs::exists(bonjourDirectoryPath))
	{
		return false;
	}

	return true;
}

bool SignManager::CheckiCloudDependencies()
{
	wchar_t* programFilesCommonDirectory;
	SHGetKnownFolderPath(FOLDERID_ProgramFilesCommon, 0, NULL, &programFilesCommonDirectory);

	fs::path deviceDriverDirectoryPath(programFilesCommonDirectory);
	deviceDriverDirectoryPath.append("Apple").append("Internet Services");

	fs::path aosKitPath(deviceDriverDirectoryPath);
	aosKitPath.append("AOSKit.dll");

	if (!fs::exists(aosKitPath))
	{
		return false;
	}

	return true;
}

void SignManager::HandleAnisetteError(AnisetteError& error)
{
	switch ((AnisetteErrorCode)error.code())
	{
	case AnisetteErrorCode::iTunesNotInstalled:
	case AnisetteErrorCode::iCloudNotInstalled:
	{
		std::string title;
		std::string message;
		std::string downloadURL;

		switch ((AnisetteErrorCode)error.code())
		{
		case AnisetteErrorCode::iTunesNotInstalled:
		{
			title = "iTunes Not Found";
			message = R"(Download the latest version of iTunes from apple.com (not the Microsoft Store) in order to continue using AltServer.

If you already have iTunes installed, please locate the "Apple" folder that was installed with iTunes. This can normally be found at:

)";

			BOOL is64Bit = false;

			if (GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process2") != NULL)
			{
				USHORT pProcessMachine = 0;
				USHORT pNativeMachine = 0;

				if (IsWow64Process2(GetCurrentProcess(), &pProcessMachine, &pNativeMachine) != 0 && pProcessMachine != IMAGE_FILE_MACHINE_UNKNOWN)
				{
					is64Bit = true;
				}
				else
				{
					is64Bit = false;
				}
			}
			else if (GetProcAddress(GetModuleHandle(TEXT("kernel32")), "IsWow64Process") != NULL)
			{
				IsWow64Process(GetCurrentProcess(), &is64Bit);
			}
			else
			{
				is64Bit = false;
			}

			if (is64Bit)
			{
				// 64-bit
				downloadURL = "https://www.apple.com/itunes/download/win64";
			}
			else
			{
				// 32-bit
				downloadURL = "https://www.apple.com/itunes/download/win32";
			}

			break;
		}

		case AnisetteErrorCode::iCloudNotInstalled:
			title = "iCloud Not Found";
			message = R"(Download the latest version of iCloud from apple.com (not the Microsoft Store) in order to continue using AltServer.

If you already have iCloud installed, please locate the "Apple" folder that was installed with iCloud. This can normally be found at:

)";
			downloadURL = "https://secure-appldnld.apple.com/windows/061-91601-20200323-974a39d0-41fc-4761-b571-318b7d9205ed/iCloudSetup.exe";
			break;
		}

		std::cout << title.c_str() << "\n";
		std::cout << message.c_str() << "\n";
		std::cout << downloadURL.c_str() << "\n";
		break;
	}

	case AnisetteErrorCode::MissingApplicationSupportFolder:
	case AnisetteErrorCode::MissingAOSKit:
	case AnisetteErrorCode::MissingFoundation:
	case AnisetteErrorCode::MissingObjc:
	{
		std::string message = "Please locate the 'Apple' folder installed with iTunes to continue using DemoSigner.\n\nThis can normally be found at:\n";
		message += this->defaultAppleFolderPath();

		std::cout << message.c_str() << "\n";
		break;
	}

	case AnisetteErrorCode::InvalidiTunesInstallation:
	{
		this->ShowAlert("Invalid iTunes Installation", error.localizedDescription());
		break;
	}
	}
}

HWND SignManager::windowHandle() const
{
	return _windowHandle;
}

HINSTANCE SignManager::instanceHandle() const
{
	return _instanceHandle;
}

bool SignManager::automaticallyLaunchAtLogin() const
{
	return false;
}

void SignManager::setAutomaticallyLaunchAtLogin(bool launch)
{
}

std::string SignManager::serverID()
{
	if (_serverID == "")
		_serverID = make_uuid();

	return _serverID;
}

void SignManager::setServerID(std::string serverID)
{
	if (_serverID == "")
		_serverID = make_uuid();
}

bool SignManager::presentedRunningNotification() const
{
	return false;
}

void SignManager::setPresentedRunningNotification(bool presentedRunningNotification)
{
}

bool SignManager::reprovisionedDevice() const
{
	return "";
}

void SignManager::setReprovisionedDevice(bool reprovisionedDevice)
{
	//SetRegistryBoolValue(REPROVISIONED_DEVICE_KEY, reprovisionedDevice);
}

std::string SignManager::defaultAppleFolderPath() const
{
	wchar_t* programFilesCommonDirectory;
	SHGetKnownFolderPath(FOLDERID_ProgramFilesCommon, 0, NULL, &programFilesCommonDirectory);

	fs::path appleDirectoryPath(programFilesCommonDirectory);
	appleDirectoryPath.append("Apple");

	return appleDirectoryPath.string();
}

std::string SignManager::appleFolderPath() const
{
	return this->defaultAppleFolderPath();
}

void SignManager::setAppleFolderPath(std::string appleFolderPath)
{
	//SetRegistryStringValue(APPLE_FOLDER_KEY, appleFolderPath);
}

std::string SignManager::internetServicesFolderPath() const
{
	fs::path internetServicesDirectoryPath(this->appleFolderPath());
	internetServicesDirectoryPath.append("Internet Services");
	return internetServicesDirectoryPath.string();
}

std::string SignManager::applicationSupportFolderPath() const
{
	fs::path applicationSupportDirectoryPath(this->appleFolderPath());
	applicationSupportDirectoryPath.append("Apple Application Support");
	return applicationSupportDirectoryPath.string();
}

static int CALLBACK BrowseFolderCallback(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	if (uMsg == BFFM_INITIALIZED)
	{
		std::string tmp = (const char*)lpData;
		odslog("Browser Path:" << tmp);
		SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);
	}

	return 0;
}


std::string SignManager::BrowseForFolder(std::wstring title, std::string folderPath)
{
	BROWSEINFO browseInfo = { 0 };
	browseInfo.lpszTitle = title.c_str();
	browseInfo.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE | BIF_NONEWFOLDERBUTTON;
	browseInfo.lpfn = BrowseFolderCallback;
	browseInfo.lParam = (LPARAM)folderPath.c_str();

	LPITEMIDLIST pidList = SHBrowseForFolder(&browseInfo);
	if (pidList == 0)
	{
		return "";
	}

	TCHAR path[MAX_PATH];
	SHGetPathFromIDList(pidList, path);

	IMalloc* imalloc = NULL;
	if (SUCCEEDED(SHGetMalloc(&imalloc)))
	{
		imalloc->Free(pidList);
		imalloc->Release();
	}

	return StringFromWideString(path);
}