/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "base/version.h"

#include "common/config-manager.h"
#include "common/events.h"
#include "common/str.h"
#include "common/system.h"
#include "common/translation.h"

#include "gui/about.h"
#include "gui/gui-manager.h"
#include "gui/message.h"
#include "gui/options.h"
#include "gui/saveload.h"
#include "gui/ThemeEngine.h"
#include "gui/ThemeEval.h"
#include "gui/widget.h"

#include "graphics/font.h"

#include "engines/dialogs.h"
#include "engines/engine.h"
#include "engines/metaengine.h"

#ifdef SMALL_SCREEN_DEVICE
#include "gui/KeysDialog.h"
#endif

class ConfigDialog : public GUI::OptionsDialog {
protected:
#ifdef SMALL_SCREEN_DEVICE
	GUI::Dialog		*_keysDialog;
#endif

public:
	ConfigDialog(bool subtitleControls);
	~ConfigDialog();

	virtual void handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data);
};

MainMenuDialog::MainMenuDialog(Engine *engine)
	: GUI::Dialog("GlobalMenu"), _engine(engine) {
	_backgroundType = GUI::ThemeEngine::kDialogBackgroundSpecial;

#ifndef DISABLE_FANCY_THEMES
	_logo = 0;
	if (g_gui.xmlEval()->getVar("Globals.ShowGlobalMenuLogo", 0) == 1 && g_gui.theme()->supportsImages()) {
		_logo = new GUI::GraphicsWidget(this, "GlobalMenu.Logo");
		_logo->useThemeTransparency(true);
		_logo->setGfx(g_gui.theme()->getImageSurface(GUI::ThemeEngine::kImageLogoSmall));
	} else {
		GUI::StaticTextWidget *title = new GUI::StaticTextWidget(this, "GlobalMenu.Title", "ScummVM");
		title->setAlign(Graphics::kTextAlignCenter);
	}
#else
	GUI::StaticTextWidget *title = new GUI::StaticTextWidget(this, "GlobalMenu.Title", "ScummVM");
	title->setAlign(Graphics::kTextAlignCenter);
#endif

	GUI::StaticTextWidget *version = new GUI::StaticTextWidget(this, "GlobalMenu.Version", gScummVMVersionDate);
	version->setAlign(Graphics::kTextAlignCenter);

	new GUI::ButtonWidget(this, "GlobalMenu.Resume", _("~R~esume"), 0, kPlayCmd, 'P');

	_loadButton = new GUI::ButtonWidget(this, "GlobalMenu.Load", _("~L~oad"), 0, kLoadCmd);
	// TODO: setEnabled -> setVisible
	_loadButton->setEnabled(_engine->hasFeature(Engine::kSupportsLoadingDuringRuntime));

	_saveButton = new GUI::ButtonWidget(this, "GlobalMenu.Save", _("~S~ave"), 0, kSaveCmd);
	// TODO: setEnabled -> setVisible
	_saveButton->setEnabled(_engine->hasFeature(Engine::kSupportsSavingDuringRuntime));

	new GUI::ButtonWidget(this, "GlobalMenu.Options", _("~O~ptions"), 0, kOptionsCmd);

	// The help button is disabled by default.
	// To enable "Help", an engine needs to use a subclass of MainMenuDialog
	// (at least for now, we might change how this works in the future).
	_helpButton = new GUI::ButtonWidget(this, "GlobalMenu.Help", _("~H~elp"), 0, kHelpCmd);

	new GUI::ButtonWidget(this, "GlobalMenu.About", _("~A~bout"), 0, kAboutCmd);

	if (g_system->getOverlayWidth() > 320)
		_rtlButton = new GUI::ButtonWidget(this, "GlobalMenu.RTL", _("~R~eturn to Launcher"), 0, kRTLCmd);
	else
		_rtlButton = new GUI::ButtonWidget(this, "GlobalMenu.RTL", _c("~R~eturn to Launcher", "lowres"), 0, kRTLCmd);
	_rtlButton->setEnabled(_engine->hasFeature(Engine::kSupportsRTL));


	new GUI::ButtonWidget(this, "GlobalMenu.Quit", _("~Q~uit"), 0, kQuitCmd);

	_aboutDialog = new GUI::AboutDialog();
	_optionsDialog = new ConfigDialog(_engine->hasFeature(Engine::kSupportsSubtitleOptions));
	_loadDialog = new GUI::SaveLoadChooser(_("Load game:"), _("Load"));
	_loadDialog->setSaveMode(false);
	_saveDialog = new GUI::SaveLoadChooser(_("Save game:"), _("Save"));
	_saveDialog->setSaveMode(true);
}

MainMenuDialog::~MainMenuDialog() {
	delete _aboutDialog;
	delete _optionsDialog;
	delete _loadDialog;
	delete _saveDialog;
}

void MainMenuDialog::handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) {
	switch (cmd) {
	case kPlayCmd:
		close();
		break;
	case kLoadCmd:
		load();
		break;
	case kSaveCmd:
		save();
		break;
	case kOptionsCmd:
		_optionsDialog->runModal();
		break;
	case kAboutCmd:
		_aboutDialog->runModal();
		break;
	case kHelpCmd: {
		GUI::MessageDialog dialog(
					_("Sorry, this engine does not currently provide in-game help. "
					"Please consult the README for basic information, and for "
					"instructions on how to obtain further assistance."));
		dialog.runModal();
		}
		break;
	case kRTLCmd: {
		Common::Event eventRTL;
		eventRTL.type = Common::EVENT_RTL;
		g_system->getEventManager()->pushEvent(eventRTL);
		close();
		}
		break;
	case kQuitCmd: {
		Common::Event eventQ;
		eventQ.type = Common::EVENT_QUIT;
		g_system->getEventManager()->pushEvent(eventQ);
		close();
		}
		break;
	default:
		GUI::Dialog::handleCommand(sender, cmd, data);
	}
}

void MainMenuDialog::reflowLayout() {
	if (_engine->hasFeature(Engine::kSupportsLoadingDuringRuntime))
		_loadButton->setEnabled(_engine->canLoadGameStateCurrently());
	if (_engine->hasFeature(Engine::kSupportsSavingDuringRuntime))
		_saveButton->setEnabled(_engine->canSaveGameStateCurrently());

	// Overlay size might have changed since the construction of the dialog.
	// Update labels when it might be needed
	// FIXME: it might be better to declare GUI::StaticTextWidget::setLabel() virtual
	// and to reimplement it in GUI::ButtonWidget to handle the hotkey.
	if (g_system->getOverlayWidth() > 320)
		_rtlButton->setLabel(_rtlButton->cleanupHotkey(_("~R~eturn to Launcher")));
	else
		_rtlButton->setLabel(_rtlButton->cleanupHotkey(_c("~R~eturn to Launcher", "lowres")));

#ifndef DISABLE_FANCY_THEMES
	if (g_gui.xmlEval()->getVar("Globals.ShowGlobalMenuLogo", 0) == 1 && g_gui.theme()->supportsImages()) {
		if (!_logo)
			_logo = new GUI::GraphicsWidget(this, "GlobalMenu.Logo");
		_logo->useThemeTransparency(true);
		_logo->setGfx(g_gui.theme()->getImageSurface(GUI::ThemeEngine::kImageLogoSmall));

		GUI::StaticTextWidget *title = (GUI::StaticTextWidget *)findWidget("GlobalMenu.Title");
		if (title) {
			removeWidget(title);
			title->setNext(0);
			delete title;
		}
	} else {
		GUI::StaticTextWidget *title = (GUI::StaticTextWidget *)findWidget("GlobalMenu.Title");
		if (!title) {
			title = new GUI::StaticTextWidget(this, "GlobalMenu.Title", "ScummVM");
			title->setAlign(Graphics::kTextAlignCenter);
		}

		if (_logo) {
			removeWidget(_logo);
			_logo->setNext(0);
			delete _logo;
			_logo = 0;
		}
	}
#endif

	Dialog::reflowLayout();
}

void MainMenuDialog::save() {
	const Common::String gameId = ConfMan.get("gameid");

	const EnginePlugin *plugin = 0;
	EngineMan.findGame(gameId, &plugin);

	int slot = _saveDialog->runModalWithPluginAndTarget(plugin, ConfMan.getActiveDomainName());

	if (slot >= 0) {
		Common::String result(_saveDialog->getResultString());
		if (result.empty()) {
			// If the user was lazy and entered no save name, come up with a default name.
			Common::String buf;
			#if defined(USE_SAVEGAME_TIMESTAMP)
			TimeDate curTime;
			g_system->getTimeAndDate(curTime);
			curTime.tm_year += 1900; // fixup year
			curTime.tm_mon++; // fixup month
			buf = Common::String::format("%04d.%02d.%02d / %02d:%02d:%02d", curTime.tm_year, curTime.tm_mon, curTime.tm_mday, curTime.tm_hour, curTime.tm_min, curTime.tm_sec);
			#else
			buf = Common::String::format("Save %d", slot + 1);
			#endif
			_engine->saveGameState(slot, buf);
		} else {
			_engine->saveGameState(slot, result);
		}

		close();
	}
}

void MainMenuDialog::load() {
	const Common::String gameId = ConfMan.get("gameid");

	const EnginePlugin *plugin = 0;
	EngineMan.findGame(gameId, &plugin);

	int slot = _loadDialog->runModalWithPluginAndTarget(plugin, ConfMan.getActiveDomainName());

	_engine->setGameToLoadSlot(slot);

	if (slot >= 0)		
		close();
}

enum {
	kKeysCmd = 'KEYS'
};

// FIXME: We use the empty string as domain name here. This tells the
// ConfigManager to use the 'default' domain for all its actions. We do that
// to get as close as possible to editing the 'active' settings.
//
// However, that requires bad & evil hacks in the ConfigManager code,
// and even then still doesn't work quite correctly.
// For example, if the transient domain contains 'false' for the 'fullscreen'
// flag, but the user used a hotkey to switch to windowed mode, then the dialog
// will display the wrong value anyway.
//
// Proposed solution consisting of multiple steps:
// 1) Add special code to the open() code that reads out everything stored
//    in the transient domain that is controlled by this dialog, and updates
//    the dialog accordingly.
// 2) Even more code is added to query the backend for current settings, like
//    the fullscreen mode flag etc., and also updates the dialog accordingly.
// 3) The domain being edited is set to the active game domain.
// 4) If the dialog is closed with the "OK" button, then we remove everything
//    stored in the transient domain (or at least everything corresponding to
//    switches in this dialog.
//    If OTOH the dialog is closed with "Cancel" we do no such thing.
//
// These changes will achieve two things at once: Allow us to get rid of using
//  "" as value for the domain, and in fact provide a somewhat better user
// experience at the same time.
ConfigDialog::ConfigDialog(bool subtitleControls)
	: GUI::OptionsDialog("", "GlobalConfig") {

	//
	// Sound controllers
	//

	addVolumeControls(this, "GlobalConfig.");
	setVolumeSettingsState(true); // could disable controls by GUI options

	//
	// Subtitle speed and toggle controllers
	//

	if (subtitleControls) {
		// Global talkspeed range of 0-255
		addSubtitleControls(this, "GlobalConfig.", 255);
		setSubtitleSettingsState(true); // could disable controls by GUI options
	}

	//
	// Add the buttons
	//

	new GUI::ButtonWidget(this, "GlobalConfig.Ok", _("~O~K"), 0, GUI::kOKCmd);
	new GUI::ButtonWidget(this, "GlobalConfig.Cancel", _("~C~ancel"), 0, GUI::kCloseCmd);

#ifdef SMALL_SCREEN_DEVICE
	new GUI::ButtonWidget(this, "GlobalConfig.Keys", _("~K~eys"), 0, kKeysCmd);
	_keysDialog = NULL;
#endif
}

ConfigDialog::~ConfigDialog() {
#ifdef SMALL_SCREEN_DEVICE
	delete _keysDialog;
#endif
}

void ConfigDialog::handleCommand(GUI::CommandSender *sender, uint32 cmd, uint32 data) {
	switch (cmd) {
	case kKeysCmd:

#ifdef SMALL_SCREEN_DEVICE
	//
	// Create the sub dialog(s)
	//
	_keysDialog = new GUI::KeysDialog();
	_keysDialog->runModal();
	delete _keysDialog;
	_keysDialog = NULL;
#endif
		break;
	default:
		GUI::OptionsDialog::handleCommand (sender, cmd, data);
	}
}
