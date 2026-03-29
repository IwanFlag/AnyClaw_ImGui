#pragma once
// lang.h — Internationalization (i18n) for AnyClaw

#include <string>

namespace anyclaw {

enum class Lang { Chinese, English };

// All string keys used by GUI and tray
enum class Str {
    // App
    AppTitle,
    // Tabs
    TabGeneral, TabAccount, TabModels, TabAbout,
    // Status
    StatusRunning, StatusDetected, StatusError, StatusNotInstalled, StatusUnknown,
    // Labels
    LabelVersion, LabelPath, LabelPort, LabelRestart, LabelViewLogs, LabelOpenDir,
    LabelAutoStart, LabelHealthInterval, LabelLanguage, LabelNotDetected, LabelInstallOc,
    // Account
    AccountTitle, AccountConnected, AccountApiKey, AccountRefreshModels, AccountLogout,
    AccountInputHint, AccountConnect, AccountNotRegistered, AccountGoRegister,
    // Models
    ModelNoModels, ModelNeedAccount, ModelCurrent, ModelSearchHint, ModelContext, ModelCount,
    // About
    AboutVersion, AboutDescription, AboutOcVersion, AboutGithub, AboutDocs, AboutCheckUpdate,
    // Wizard
    WizardTitle, WizardLanguageTitle, WizardLanguagePrompt, WizardNext, WizardPrev,
    WizardStep2, WizardDetected, WizardNotDetected, WizardInstallNpm, WizardInstallExe,
    WizardSkip, WizardStep3, WizardInputApiKey, WizardComplete, WizardNoAccount,
    WizardGoRegister, WizardSelectModel, WizardFinish,
    // Notifications
    NotifySelfCheckRunning, NotifyInstallSuccess, NotifyGatewayRestarting,
    NotifyGatewayRestarted, NotifySettingsSaved,
    // Errors
    ErrOpenClawNotInstalled, ErrNodeNotFound, ErrSelfHealing, ErrInstallFailed,
    ErrNetworkError, ErrApiKeyInvalid,
    // Buttons
    Save, Cancel,
    // Tray menu
    TrayOpenSettings, TrayRestart, TrayViewLogs, TrayAutoStart, TrayAbout, TrayExit,
    // Tray balloons
    BalloonRecovered, BalloonOffline, BalloonSettingsSaved, BalloonInstallSuccess,
    BalloonStartFailed, BalloonApiKeyInvalid, BalloonNetworkError,

    COUNT // marker
};

// Get localized string
const char* S(Str key, Lang lang);

// Get language display name
const char* lang_name(Lang lang);

// Detect system language
Lang detect_system_language();

} // namespace anyclaw
