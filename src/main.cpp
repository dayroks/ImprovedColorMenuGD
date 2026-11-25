// Improved Color Menu
// Замінює панель "Base Color" / "Detail Color" (CustomizeObjectLayer) на панель зі сторінками:
//   [ Special ]  [ Page N / M ]  [ Recent ]
//
// ВАЖЛИВО: ця панель належить класу CustomizeObjectLayer (вкладки Base / Detail / Text),
// а не ColorSelectPopup (той — це колесо вибору кольору для самого каналу).
// Для знаходження ванільних меню потрібен мод geode.node-ids (див. mod.json).

#include <Geode/Geode.hpp>
#include <Geode/modify/CustomizeObjectLayer.hpp>
#include <algorithm>
#include <array>
#include <functional>
#include <string>
#include <vector>

using namespace geode::prelude;

namespace {
    constexpr int MAX_CHANNEL = 999;
    constexpr int PER_PAGE = 35; // 7 x 5
    constexpr int PAGE_COUNT = (MAX_CHANNEL + PER_PAGE - 1) / PER_PAGE;
    constexpr size_t RECENT_COUNT = 15; // 3 x 5

    // Порядок спец-каналів як на скріншоті: D P1 P2 / BC C1 MC / LBC C2 MC2 / OBJ L 3DL / LTR чорний білий
    constexpr std::array<int, 15> SPECIAL_ORDER {
        0,    1005, 1006,
        1000, 1001, 1013,
        1007, 1009, 1014,
        1004, 1002, 1003,
        1012, 1010, 1011,
    };

    int s_page = 0;           // остання відкрита сторінка (живе між відкриттями вікна)
    int s_pending = 0;        // канал, який треба додати в Recent при закритті
    std::vector<int> s_recent;
    bool s_recentLoaded = false;

    std::string shortName(int channel) {
        switch (channel) {
            case 0: return "D";
            case 1000: return "BC";
            case 1001: return "C1";
            case 1002: return "L";
            case 1003: return "3DL";
            case 1004: return "OBJ";
            case 1005: return "P1";
            case 1006: return "P2";
            case 1007: return "LBC";
            case 1009: return "C2";
            case 1010: return "";
            case 1011: return "";
            case 1012: return "LTR";
            case 1013: return "MC";
            case 1014: return "MC2";
            default: return std::to_string(channel);
        }
    }

    std::string longName(int channel) {
        if (channel > 0 && channel <= MAX_CHANNEL) return std::to_string(channel);
        return GJSpecialColorSelect::textForColorIdx(channel);
    }

    void loadRecent() {
        if (s_recentLoaded) return;
        s_recentLoaded = true;
        s_recent = Mod::get()->getSavedValue<std::vector<int>>("recent");
        if (s_recent.size() > RECENT_COUNT) s_recent.resize(RECENT_COUNT);
    }

    void commitPending() {
        if (s_pending == 0) return;
        loadRecent();
        s_recent.erase(std::remove(s_recent.begin(), s_recent.end(), s_pending), s_recent.end());
        s_recent.insert(s_recent.begin(), s_pending);
        if (s_recent.size() > RECENT_COUNT) s_recent.resize(RECENT_COUNT);
        Mod::get()->setSavedValue("recent", s_recent);
        s_pending = 0;
    }

    RowLayout* makeGridLayout() {
        auto layout = RowLayout::create();
        layout->setCrossAxisOverflow(false);
        layout->setGrowCrossAxis(true);
        layout->setAxisAlignment(AxisAlignment::Center);
        layout->setCrossAxisAlignment(AxisAlignment::Center);
        layout->setGap(4.f);
        return layout;
    }
}

class $modify(ICMCustomizeObjectLayer, CustomizeObjectLayer) {
    struct Fields {
        bool initDone = false;
    };

    // ---------- допоміжне ----------

    static ColorAction* actionFor(int channel) {
        auto editor = LevelEditorLayer::get();
        if (!editor || !editor->m_levelSettings || !editor->m_levelSettings->m_effectManager) return nullptr;
        return editor->m_levelSettings->m_effectManager->getColorAction(channel);
    }

    void refreshSprite(ColorChannelSprite* sprite) {
        int channel = static_cast<int>(reinterpret_cast<intptr_t>(sprite->getUserData()));
        if (channel == -1) return; // порожній слот у Recent
        auto action = actionFor(channel);
        if (!action) return;
        sprite->updateValues(action);

        switch (channel) {
            case 1010: sprite->setColor({ 0, 0, 0 }); break;
            case 0: sprite->setOpacity(50); break;
            case 1007: sprite->setOpacity(120); break;
        }
        if (auto label = sprite->getChildByID("id-label")) {
            label->setVisible(!action->m_copyID);
        }
    }

    ColorChannelSprite* makeSprite(int channel, bool recentSlot) {
        auto spr = ColorChannelSprite::create();
        spr->setScale(.8f);
        // tag перезаписується CCMenuItemSpriteExtra, тому канал зберігаємо в userData
        spr->setUserData(reinterpret_cast<void*>(static_cast<intptr_t>(channel)));
        spr->setID("channel-sprite"_spr);

        if (recentSlot && channel == 0) {
            // порожній слот
            spr->setOpacity(105);
            spr->setColor({ 20, 20, 20 });
            spr->setUserData(reinterpret_cast<void*>(static_cast<intptr_t>(-1)));
            return spr;
        }

        auto sel = CCSprite::createWithSpriteFrameName("GJ_select_001.png");
        sel->setScale(1.1f);
        sel->setID("selected-indicator");
        sel->setPosition(spr->getContentSize() / 2);
        sel->setVisible(false);
        spr->addChild(sel);

        auto label = CCLabelBMFont::create(shortName(channel).c_str(), "bigFont.fnt");
        label->limitLabelWidth(25.f, .4f, .2f);
        label->setPosition(spr->getContentSize() / 2);
        label->setID("id-label");
        spr->addChild(label);

        this->refreshSprite(spr);
        return spr;
    }

    CCMenuItemSpriteExtra* makeButton(int channel, bool recentSlot = false) {
        auto btn = CCMenuItemSpriteExtra::create(
            this->makeSprite(channel, recentSlot),
            this, menu_selector(CustomizeObjectLayer::onSelectColor)
        );
        btn->setTag(channel);
        btn->setID(fmt::format("channel-{}-button", channel));
        if (recentSlot && channel == 0) btn->setEnabled(false);
        return btn;
    }

    void forEachButton(std::string const& id, std::function<void(CCMenuItemSpriteExtra*)> fn) {
        auto menu = m_mainLayer->getChildByID(id);
        if (!menu) return;
        for (auto child : CCArrayExt<CCMenuItemSpriteExtra*>(menu->getChildren())) {
            fn(child);
        }
    }

    void refreshAllSprites() {
        auto refresh = [this](CCMenuItemSpriteExtra* b) {
            if (auto spr = typeinfo_cast<ColorChannelSprite*>(b->getChildByID("channel-sprite"_spr))) {
                this->refreshSprite(spr);
            }
        };
        this->forEachButton("channels-menu", refresh);
        this->forEachButton("special-channels-menu", refresh);
        this->forEachButton("recent-channels-menu"_spr, refresh);
    }

    void markSelected() {
        int selected = this->getActiveMode(true);
        auto mark = [selected](CCMenuItemSpriteExtra* b) {
            if (auto spr = b->getChildByID("channel-sprite"_spr)) {
                if (auto ind = spr->getChildByID("selected-indicator")) {
                    ind->setVisible(b->getTag() == selected);
                }
            }
        };
        this->forEachButton("channels-menu", mark);
        this->forEachButton("special-channels-menu", mark);
        this->forEachButton("recent-channels-menu"_spr, mark);
    }

    void gotoPage(int page) {
        if (!m_fields->initDone) return;
        auto menu = m_mainLayer->getChildByID("channels-menu");
        if (!menu) return;

        page = std::clamp(page, 0, PAGE_COUNT - 1);
        s_page = page;

        menu->removeAllChildren();
        for (int i = 1; i <= PER_PAGE; i++) {
            int channel = page * PER_PAGE + i;
            if (channel > MAX_CHANNEL) break;
            menu->addChild(this->makeButton(channel));
        }
        menu->updateLayout();

        if (auto pageMenu = m_mainLayer->getChildByID("page-menu"_spr)) {
            if (auto label = typeinfo_cast<CCLabelBMFont*>(pageMenu->getChildByID("current-page"))) {
                label->setString(fmt::format("Page {} / {}", page + 1, PAGE_COUNT).c_str());
            }
        }
        this->markSelected();
    }

    void gotoPageWithChannel(int channel) {
        if (channel > 0 && channel <= MAX_CHANNEL) {
            this->gotoPage((channel - 1) / PER_PAGE);
        }
    }

    void onICMPage(CCObject* sender) {
        this->gotoPage(s_page + sender->getTag());
    }

    // Те, що треба зробити після зміни каналу НЕ через onSelectColor (стрілки, текстове поле).
    void channelChanged(int channel) {
        m_customColorChannel = channel;
        s_pending = channel;
        this->gotoPageWithChannel(channel);
    }

    // ---------- перевизначення ванільних методів ----------

    $override
    void onSelectColor(CCObject* sender) {
        if (!m_fields->initDone) return CustomizeObjectLayer::onSelectColor(sender);

        // gotoPage видаляє кнопки, у тому числі саму sender — тримаємо її живою
        auto keepAlive = Ref(sender);
        int channel = sender->getTag();
        CustomizeObjectLayer::onSelectColor(sender);
        if (!channel) channel = this->getActiveMode(true); // "Default" -> реальний канал

        m_customColorChannel = channel;
        this->updateCustomColorLabels();
        this->gotoPageWithChannel(channel);
        if (channel != 0) s_pending = channel;
    }

    $override
    void highlightSelected(ButtonSprite* sprite) {
        if (!m_fields->initDone) return CustomizeObjectLayer::highlightSelected(sprite);
        this->markSelected();
    }

    $override
    void updateColorSprite() {
        CustomizeObjectLayer::updateColorSprite();
        if (m_fields->initDone) this->refreshAllSprites();
    }

    $override
    void updateCustomColorLabels() {
        if (!m_fields->initDone) return CustomizeObjectLayer::updateCustomColorLabels();
        // не даємо textChanged спрацювати від нашого setString
        auto delegate = m_customColorInput->m_delegate;
        m_customColorInput->setDelegate(nullptr);
        m_customColorInput->setString(longName(m_customColorChannel));
        m_customColorInput->setDelegate(delegate);
        m_customColorInput->m_textField->detachWithIME();
    }

    // стрілки вгору / вниз біля поля каналу (tag 1 = вгору)
    $override
    void onUpdateCustomColor(CCObject* sender) {
        if (!m_fields->initDone) return CustomizeObjectLayer::onUpdateCustomColor(sender);

        int c = m_customColorChannel;
        if (c < 1 || c > MAX_CHANNEL) c = 1;
        else if (sender->getTag() == 1) c = std::min(c + 1, MAX_CHANNEL);
        else c = std::max(c - 1, 1);

        m_customColorSelected = true;
        m_customColorChannel = c;
        this->updateCustomColorLabels();
        this->updateSelected(c);
        m_customColorSelected = false;
        this->updateColorSprite();
        this->updateChannelLabel(c);
        this->highlightSelected(nullptr);
        this->channelChanged(c);
    }

    $override
    void onNextColorChannel(CCObject* sender) {
        CustomizeObjectLayer::onNextColorChannel(sender);
        if (m_fields->initDone) this->channelChanged(m_customColorChannel);
    }

    $override
    void textChanged(CCTextInputNode* input) {
        CustomizeObjectLayer::textChanged(input);
        if (m_fields->initDone) this->channelChanged(m_customColorChannel);
    }

    $override
    void onClose(CCObject* sender) {
        commitPending();
        CustomizeObjectLayer::onClose(sender);
    }

    // ---------- побудова панелі ----------

    $override
    bool init(GameObject* obj, CCArray* objs) {
        if (!CustomizeObjectLayer::init(obj, objs)) return false;

        commitPending(); // якщо минулого разу вікно закрили без onClose

        auto channelsMenu = m_mainLayer->getChildByID("channels-menu");
        auto specialsMenu = m_mainLayer->getChildByID("special-channels-menu");
        if (!channelsMenu || !specialsMenu || !m_colorButtons || !m_colorTabNodes) {
            // немає node-ids або інша версія GD — залишаємо ванільну панель
            log::warn("Improved Color Menu: vanilla menus not found, is geode.node-ids loaded?");
            return true;
        }

        loadRecent();
        auto winSize = CCDirector::get()->getWinSize();

        // нижні/бокові меню підтягуємо під ширшу панель
        if (auto m = m_mainLayer->getChildByID("browse-menu")) m->setPositionY(winSize.height / 2 - 50.f);
        if (auto m = m_mainLayer->getChildByID("copy-paste-menu")) m->setPositionY(winSize.height / 2 + 50.f);
        if (auto m = m_mainLayer->getChildByID("selected-channel-menu")) {
            m->setPosition(winSize.width / 2 + 125.f, winSize.height / 2 - 95.f);
            m->setScale(.8f);
        }
        CCPoint selectPos { winSize.width / 2 + 210.f, winSize.height / 2 - 15.f };
        if (auto m = m_mainLayer->getChildByID("select-channel-menu")) m->setPosition(selectPos);
        if (auto m = m_mainLayer->getChildByID("channel-input-bg")) m->setPosition(selectPos);
        if (auto m = m_mainLayer->getChildByID("channel-input")) m->setPosition(selectPos);
        m_customColorInput->m_maxLabelWidth = 38.f;

        // прибираємо ванільні кнопки каналів
        m_colorButtons->removeAllObjects();
        channelsMenu->removeAllChildren();
        specialsMenu->removeAllChildren();

        // темна підкладка
        auto bg = CCScale9Sprite::create("square02_001.png", { 0, 0, 80, 80 });
        bg->setOpacity(80);
        bg->setContentSize({ 340.f, 150.f });
        bg->setPosition(winSize / 2);
        bg->setID("bg"_spr);
        m_mainLayer->addChild(bg);
        m_colorTabNodes->addObject(bg); // ховається на вкладці Text

        float const midY = winSize.height / 2 - 7.5f;
        float const titleY = midY + 72.f;

        auto addTitle = [&](char const* text, float x, std::string const& id) {
            auto t = CCLabelBMFont::create(text, "bigFont.fnt");
            t->setScale(.35f);
            t->setPosition(x, titleY);
            t->setID(id);
            m_mainLayer->addChild(t);
            m_colorTabNodes->addObject(t);
        };
        auto addLine = [&](float x, std::string const& id) {
            auto l = CCSprite::createWithSpriteFrameName("edit_vLine_001.png");
            l->setPosition({ x, midY });
            l->setScaleY(1.65f);
            l->setOpacity(100);
            l->setID(id);
            m_mainLayer->addChild(l);
            m_colorTabNodes->addObject(l);
        };

        // --- Special (ліворуч) ---
        float const specialX = winSize.width / 2 - 130.f;
        specialsMenu->setContentSize({ 90.f, 125.f });
        specialsMenu->setPosition(specialX, midY);
        specialsMenu->setLayout(makeGridLayout());
        for (int ch : SPECIAL_ORDER) specialsMenu->addChild(this->makeButton(ch));
        specialsMenu->updateLayout();
        addTitle("Special", specialX, "special-title"_spr);

        // --- Page (центр) ---
        float const pagesX = winSize.width / 2;
        channelsMenu->setContentSize({ 170.f, 125.f });
        channelsMenu->setPosition(pagesX, midY);
        channelsMenu->setLayout(makeGridLayout());

        auto pageMenu = CCMenu::create();
        pageMenu->setID("page-menu"_spr);
        pageMenu->ignoreAnchorPointForPosition(false);
        pageMenu->setContentSize({ 150.f, 20.f });
        pageMenu->setPosition(pagesX, titleY);

        auto pageLabel = CCLabelBMFont::create("Page 1 / 1", "bigFont.fnt");
        pageLabel->setScale(.35f);
        pageLabel->setPosition(70.f, 10.f);
        pageLabel->setID("current-page");
        pageMenu->addChild(pageLabel);

        for (int dir : { -1, 1 }) {
            auto spr = CCSprite::createWithSpriteFrameName(dir < 0 ? "edit_leftBtn_001.png" : "edit_rightBtn_001.png");
            spr->setScale(.5f);
            auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(ICMCustomizeObjectLayer::onICMPage));
            btn->setTag(dir);
            btn->setPosition(dir < 0 ? 10.f : 140.f, 10.f);
            pageMenu->addChild(btn);
        }
        m_mainLayer->addChild(pageMenu);
        m_colorTabNodes->addObject(pageMenu);

        addLine(pagesX - 85.f - 4.f, "separator-1"_spr);
        addLine(pagesX + 85.f + 4.f, "separator-2"_spr);

        // --- Recent (праворуч) ---
        float const recentX = winSize.width / 2 + 130.f;
        auto recentMenu = CCMenu::create();
        recentMenu->setID("recent-channels-menu"_spr);
        recentMenu->ignoreAnchorPointForPosition(false);
        recentMenu->setContentSize({ 90.f, 125.f });
        recentMenu->setPosition(recentX, midY);
        recentMenu->setLayout(makeGridLayout());
        for (size_t i = 0; i < RECENT_COUNT; i++) {
            bool has = i < s_recent.size();
            recentMenu->addChild(this->makeButton(has ? s_recent[i] : 0, true));
        }
        recentMenu->updateLayout();
        m_mainLayer->addChild(recentMenu);
        m_colorTabNodes->addObject(recentMenu);
        addTitle("Recent", recentX, "recent-title"_spr);

        m_fields->initDone = true;
        this->gotoPage(s_page);
        this->gotoPageWithChannel(m_customColorChannel);
        this->updateCustomColorLabels();
        return true;
    }
};
