#include <Geode/Geode.hpp>
#include <Geode/modify/GameManager.hpp>
#include <Geode/modify/MenuLayer.hpp>
#include <Geode/modify/PauseLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/ui/GeodeUI.hpp>
#include <algorithm>

#include "Options.hpp"

using namespace geode::prelude;

// ---------------------------------------------------------------- helpers

void hbv::applySpeed() {
	if (auto director = CCDirector::get()) {
		if (auto scheduler = director->getScheduler()) {
			scheduler->setTimeScale(hbv::speed());
		}
	}
}

void hbv::openMenu() {
	geode::openSettingsPopup(Mod::get());
}

$on_mod(Loaded) {
	listenForSettingChanges<bool>("speedhack", [](bool) { hbv::applySpeed(); });
	listenForSettingChanges<double>("speed", [](double) { hbv::applySpeed(); });
}

// -------------------------------------------------------------- icon hack

class $modify(HBVGameManager, GameManager) {
	bool isIconUnlocked(int id, IconType type) {
		if (hbv::on("icon-hack")) return true;
		return GameManager::isIconUnlocked(id, type);
	}

	bool isColorUnlocked(int id, UnlockType type) {
		if (hbv::on("icon-hack")) return true;
		return GameManager::isColorUnlocked(id, type);
	}
};

// ------------------------------------------------------------ menu buttons

class $modify(HBVMenuLayer, MenuLayer) {
	bool init() {
		if (!MenuLayer::init()) return false;

		auto sprite = ButtonSprite::create("HBV");
		sprite->setScale(0.8f);
		auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(HBVMenuLayer::onHBV));
		button->setID("hbv-menu-button"_spr);

		if (auto menu = this->getChildByID("bottom-menu")) {
			menu->addChild(button);
			menu->updateLayout();
		}
		return true;
	}

	void onHBV(CCObject*) {
		hbv::openMenu();
	}
};

class $modify(HBVPauseLayer, PauseLayer) {
	void customSetup() {
		PauseLayer::customSetup();

		auto win = CCDirector::get()->getWinSize();
		auto menu = CCMenu::create();
		menu->setPosition({win.width - 45.f, win.height - 25.f});

		auto sprite = ButtonSprite::create("HBV");
		sprite->setScale(0.7f);
		auto button = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(HBVPauseLayer::onHBV));
		button->setID("hbv-menu-button"_spr);
		menu->addChild(button);

		this->addChild(menu);
	}

	void onHBV(CCObject*) {
		hbv::openMenu();
	}
};

// -------------------------------------------------------------- play layer

class $modify(HBVPlayLayer, PlayLayer) {
	struct Fields {
		CCLabelBMFont* hud = nullptr;
		CCLabelBMFont* startLabel = nullptr;
		CCMenu* startMenu = nullptr;
		std::vector<StartPosObject*> starts;
		int startIndex = 0;      // 0 = the level's original start
		int frames = 0;
		int hitFrames = 0;       // frames where noclip saved you
		int lastHitFrame = -1;
		bool hitboxesOn = false;
	};

	void setupHasCompleted() {
		PlayLayer::setupHasCompleted();
		hbv::applySpeed();
		this->buildHud();
		this->buildStartPosSwitcher();
	}

	void onQuit() {
		PlayLayer::onQuit();
		if (auto scheduler = CCDirector::get()->getScheduler()) {
			scheduler->setTimeScale(1.f);
		}
	}

	// Noclip: swallow deaths, except the anti-cheat spike.
	void destroyPlayer(PlayerObject* player, GameObject* object) {
		if (hbv::on("noclip") && object != m_anticheatSpike) {
			if (m_fields->lastHitFrame != m_fields->frames) {
				m_fields->lastHitFrame = m_fields->frames;
				m_fields->hitFrames++;
			}
			return;
		}
		PlayLayer::destroyPlayer(player, object);
	}

	// Safe mode: a cheated completion is treated like a test run (not saved).
	void levelComplete() {
		if (hbv::on("safe-mode") && hbv::cheating()) {
			m_isTestMode = true;
		}
		PlayLayer::levelComplete();
	}

	void postUpdate(float dt) {
		PlayLayer::postUpdate(dt);
		m_fields->frames++;
		this->updateHitboxes();
		this->updateHud();
	}

	// ---- HUD

	void buildHud() {
		auto win = CCDirector::get()->getWinSize();
		auto label = CCLabelBMFont::create("", "bigFont.fnt");
		label->setScale(0.35f);
		label->setAnchorPoint({0.f, 1.f});
		label->setPosition({6.f, win.height - 6.f});
		label->setOpacity(170);
		label->setZOrder(100);
		m_uiLayer->addChild(label);
		m_fields->hud = label;
	}

	void updateHud() {
		auto label = m_fields->hud;
		if (!label) return;

		bool showLabel = hbv::on("labels");
		label->setVisible(showLabel);
		if (m_fields->startMenu) {
			m_fields->startMenu->setVisible(hbv::on("startpos-switcher"));
		}
		if (!showLabel) return;

		float spf = CCDirector::get()->getSecondsPerFrame();
		float fps = spf > 0.f ? 1.f / spf : 0.f;
		std::string text = fmt::format("FPS {:.0f}", fps);

		if (hbv::on("noclip") && m_fields->frames > 0) {
			float accuracy = 100.f * (1.f - static_cast<float>(m_fields->hitFrames) / m_fields->frames);
			text += fmt::format("  |  Acc {:.2f}%", accuracy);
		}
		if (hbv::cheating()) {
			text += "  |  CHEATS";
			label->setColor({255, 90, 90});
		} else {
			label->setColor({255, 255, 255});
		}
		label->setString(text.c_str());
	}

	// ---- Hitboxes

	void updateHitboxes() {
		if (hbv::on("hitboxes")) {
			m_isDebugDrawEnabled = true;
			m_fields->hitboxesOn = true;
			this->updateDebugDraw();
		} else if (m_fields->hitboxesOn) {
			m_fields->hitboxesOn = false;
			m_isDebugDrawEnabled = false;
			if (m_debugDrawNode) m_debugDrawNode->clear();
		}
	}

	// ---- Startpos switcher

	void buildStartPosSwitcher() {
		auto& starts = m_fields->starts;
		starts.clear();
		for (auto obj : CCArrayExt<GameObject*>(m_objects)) {
			if (auto startPos = typeinfo_cast<StartPosObject*>(obj)) {
				starts.push_back(startPos);
			}
		}
		if (starts.empty()) return;

		std::sort(starts.begin(), starts.end(), [](StartPosObject* a, StartPosObject* b) {
			return a->getPositionX() < b->getPositionX();
		});

		auto win = CCDirector::get()->getWinSize();
		auto menu = CCMenu::create();
		menu->setPosition({win.width / 2.f, 22.f});

		auto leftSprite = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
		leftSprite->setScale(0.6f);
		auto rightSprite = CCSprite::createWithSpriteFrameName("GJ_arrow_01_001.png");
		rightSprite->setScale(0.6f);
		rightSprite->setFlipX(true);

		auto left = CCMenuItemSpriteExtra::create(leftSprite, this, menu_selector(HBVPlayLayer::onStartPos));
		left->setTag(-1);
		left->setPosition({-70.f, 0.f});
		auto right = CCMenuItemSpriteExtra::create(rightSprite, this, menu_selector(HBVPlayLayer::onStartPos));
		right->setTag(1);
		right->setPosition({70.f, 0.f});
		menu->addChild(left);
		menu->addChild(right);

		auto label = CCLabelBMFont::create("Start: Original", "bigFont.fnt");
		label->setScale(0.4f);
		menu->addChild(label);

		m_uiLayer->addChild(menu);
		m_fields->startMenu = menu;
		m_fields->startLabel = label;
	}

	void onStartPos(CCObject* sender) {
		auto& starts = m_fields->starts;
		if (starts.empty()) return;

		int count = static_cast<int>(starts.size()) + 1;
		int step = static_cast<CCNode*>(sender)->getTag();
		m_fields->startIndex = ((m_fields->startIndex + step) % count + count) % count;

		StartPosObject* target = nullptr;
		if (m_fields->startIndex > 0) {
			target = starts[m_fields->startIndex - 1];
		}
		this->setStartPosObject(target);

		m_fields->startLabel->setString(
			m_fields->startIndex == 0
				? "Start: Original"
				: fmt::format("Start: {}/{}", m_fields->startIndex, starts.size()).c_str()
		);
		this->resetLevel();
	}
};
