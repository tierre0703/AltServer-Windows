#pragma once

#include <string>

#include "Account.hpp"
#include "AppID.hpp"
#include "Application.hpp"
#include "Certificate.hpp"
#include "Device.hpp"
#include "ProvisioningProfile.hpp"
#include "Team.hpp"

#include "AppleAPISession.h"

#include <pplx/pplxtasks.h>

#ifdef _WIN32
#include <filesystem>
#undef _WINSOCKAPI_
#define _WINSOCKAPI_  /* prevents <winsock.h> inclusion by <windows.h> */
#include <windows.h>
namespace fs = std::filesystem;
#else
#include <boost/filesystem.hpp>
namespace fs = boost::filesystem;
#endif

class SignManager
{
public:
	static SignManager* instance();

	pplx::task<void> SignProc(std::shared_ptr<Device> device, std::string appleID, std::string password);

	bool reprovisionedDevice() const;
	void setReprovisionedDevice(bool reprovisionedDevice);
	std::string serverID() const;
	void setServerID(std::string serverID);

	SignManager();
	~SignManager();

private:

	std::string _serverID = "s";
	bool _reprovisionedDevice = false;

	static SignManager* _instance;
	pplx::task<fs::path> DownloadApp();

	pplx::task<void> _SignProc(std::shared_ptr<Device> installDevice, std::string appleID, std::string password);
	bool CheckDependencies();
	bool CheckiCloudDependencies();

	pplx::task<std::pair<std::shared_ptr<Account>, std::shared_ptr<AppleAPISession>>>  Authenticate(std::string appleID, std::string password, std::shared_ptr<AnisetteData> anisetteData);
	pplx::task<std::shared_ptr<Team>> FetchTeam(std::shared_ptr<Account> account, std::shared_ptr<AppleAPISession> session);
	pplx::task<std::shared_ptr<Certificate>> FetchCertificate(std::shared_ptr<Team> team, std::shared_ptr<AppleAPISession> session);
	pplx::task<std::shared_ptr<AppID>> RegisterAppID(std::string appName, std::string identifier, std::shared_ptr<Team> team, std::shared_ptr<AppleAPISession> session);
	pplx::task<std::shared_ptr<Device>> RegisterDevice(std::shared_ptr<Device> device, std::shared_ptr<Team> team, std::shared_ptr<AppleAPISession> session);
	pplx::task<std::shared_ptr<ProvisioningProfile>> FetchProvisioningProfile(std::shared_ptr<AppID> appID, std::shared_ptr<Team> team, std::shared_ptr<AppleAPISession> session);


	pplx::task<void> InstallApp(std::shared_ptr<Application> app,
		std::shared_ptr<Device> device,
		std::shared_ptr<Team> team,
		std::shared_ptr<AppID> appID,
		std::shared_ptr<Certificate> certificate,
		std::shared_ptr<ProvisioningProfile> profile);


};

