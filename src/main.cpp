#include <Geode/Geode.hpp>
#include <Geode/modify/ColorSelectPopup.hpp>
#include <Geode/ui/ScrollLayer.hpp>
#include <algorithm>

using namespace geode::prelude;

// Scrollable list of all color channels with live previews
class ChannelListPopup : public geode::Popup {
protected:
    Ref<ColorSelectPopup> m_owner;

    bool init(ColorSelectPopup* owner) {
        if (!Popup::init(380.f, 270.f)) return false;
        m_owner = owner;
        this->setTitle("Color Channels");

        auto editor = LevelEditorLayer::get();
        if (!editor || !editor->m_effectManager) return true;

        // collect + sort channels
        std::vector<ColorAction*> actions;
        for (auto act : CCArrayExt<ColorAction*>(editor->m_effectManager->getAllColorActions())) {
            actions.push_back(act);
        }
        std::sort(actions.begin(), actions.end(), [](auto a, auto b) {
            return a->m_colorID < b->m_colorID;
        });

        constexpr int cols = 7;
        constexpr float cell = 46.f;
        CCSize viewSize { cols * cell, 190.f };
        int rows = (static_cast<int>(actions.size()) + cols - 1) / cols;
        float contentH = std::max(viewSize.height, rows * cell);

        auto scroll = ScrollLayer::create(viewSize);
        scroll->setAnchorPoint({ .5f, .5f });
        scroll->ignoreAnchorPointForPosition(false);
        scroll->setPosition(m_mainLayer->getContentSize() / 2 - CCPoint { 0.f, 12.f });
        m_mainLayer->addChild(scroll);

        auto menu = CCMenu::create();
        menu->setPosition(0.f, 0.f);
        menu->setContentSize({ viewSize.width, contentH });
        scroll->m_contentLayer->addChild(menu);
        scroll->m_contentLayer->setContentSize({ viewSize.width, contentH });

        int current = owner->m_colorID;
        for (size_t i = 0; i < actions.size(); i++) {
            auto act = actions[i];
            int col = i % cols;
            int row = i / cols;

            auto spr = ColorChannelSprite::create();
            spr->updateValues(act);
            float target = cell * .8f;
            spr->setScale(target / std::max(1.f, spr->getContentSize().width));

            auto lbl = CCLabelBMFont::create(std::to_string(act->m_colorID).c_str(), "bigFont.fnt");
            lbl->setScale(.4f);
            lbl->setPosition(spr->getContentSize() / 2);
            spr->addChild(lbl);
            if (act->m_colorID == current) {
                lbl->setColor({ 255, 255, 0 });
            }

            auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(ChannelListPopup::onPick));
            btn->setTag(act->m_colorID);
            btn->setPosition(col * cell + cell / 2, contentH - row * cell - cell / 2);
            menu->addChild(btn);
        }
        scroll->scrollToTop();
        return true;
    }

    void onPick(CCObject* sender) {
        auto item = static_cast<CCNode*>(sender);
        if (m_owner) {
            m_owner->onUpdateCustomColor(item); // reads sender tag = channel id
        }
        this->onClose(nullptr);
    }

public:
    static ChannelListPopup* create(ColorSelectPopup* owner) {
        auto ret = new ChannelListPopup();
        if (ret->init(owner)) {
            ret->autorelease();
            return ret;
        }
        delete ret;
        return nullptr;
    }
};

class $modify(BCMColorSelectPopup, ColorSelectPopup) {
    bool init(EffectGameObject* object, CCArray* objects, ColorAction* action) {
        if (!ColorSelectPopup::init(object, objects, action)) return false;
        if (!m_buttonMenu) return true;

        auto spr = ButtonSprite::create("All", 40, true, "goldFont.fnt", "GJ_button_04.png", 26.f, .6f);
        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(BCMColorSelectPopup::onAllChannels));
        btn->setID("all-channels-button"_spr);

        auto size = CCDirector::get()->getWinSize();
        // bottom-right area of the popup, near the vanilla "+" button
        btn->setPosition(m_buttonMenu->convertToNodeSpace({ size.width / 2 + 125.f, size.height / 2 - 52.f }));
        m_buttonMenu->addChild(btn);
        return true;
    }

    void onAllChannels(CCObject*) {
        if (auto popup = ChannelListPopup::create(this)) popup->show();
    }
};
