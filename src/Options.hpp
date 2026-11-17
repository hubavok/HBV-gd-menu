#pragma once

#include <Geode/Geode.hpp>

namespace hbv {
	inline bool on(char const* key) {
		return geode::Mod::get()->getSettingValue<bool>(key);
	}

	inline double num(char const* key) {
		return geode::Mod::get()->getSettingValue<double>(key);
	}

	inline float speed() {
		return on("speedhack") ? static_cast<float>(num("speed")) : 1.f;
	}

	// Anything that would make a completion illegitimate.
	inline bool cheating() {
		return on("noclip") || speed() != 1.f;
	}

	// Pushes the current speed setting to the cocos scheduler.
	void applySpeed();

	// Opens the HBV Menu (the mod's settings popup).
	void openMenu();
}
