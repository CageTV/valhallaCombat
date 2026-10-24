#pragma once

/* Valhalla Combat's settings in the SKSE Menu Framework (replaces the MCM Helper menu, so SkyUI and MCM Helper are not needed).
The pages are generated from the old MCM config (tools/gen_menu_table.py -> include/MenuTable.h); the values live in
Data/SKSE/Plugins/ValhallaCombat/Settings.ini. */
namespace VCMenu
{
	/* Adds the pages when SKSE Menu Framework is installed. Call at kPostLoad, after the settings were read. */
	void Register();

	/* True while a key bind is waiting for the next key press. */
	bool CaptureActive();

	/* Called by the input handler with the key code (keyboard scan code, mouse button + 256, gamepad index) of the next key press. */
	void OnKeyCaptured(int a_key);

	/* Copies the old MCM Helper settings file to the new settings file once, if only the old one exists. */
	void EnsureSettingsFile();
}
