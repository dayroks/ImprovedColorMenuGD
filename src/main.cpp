#include <Geode/Geode.hpp>
#include <Geode/modify/ColorSelectPopup.hpp>
#include <algorithm>
#include <map>

using namespace geode::prelude;

// ===== налаштування =====
static constexpr int COLS = 5;               // колонок кольорів на сторінці
static constexpr int ROWS = 4;               // рядків (5 x 4 = 20 іконок)
static constexpr int PER_PAGE = COLS * ROWS;
static constexpr int MAX_CHANNEL = 999;
static constexpr int MAX_RECENT = 12;        // 3 x 4
// ========================

struct SpecialDef {
    const char* label;
    int id; // >=1000: спец-канал; -1..-4: оригінальна кнопка ванілі (Default, P-Col1, P-Col2, Light BG)
};

static const SpecialDef SPECIALS[15] = {
    {"D", -1},   {"P1", -2},  {"P2", -3},
    {"BC", 1000},{"C1", 1001},{"MC", 1013},
    {"LBC", -4}, {"C2", 1009},{"MC2", 1014},
    {"OBJ", 1004},{"L", 1002},{"3DL", 1003},
    {"LTR", 1012},{"", 1010}, {"", 1011},
};

class ColorPanel : public CCNode {
protected:
    ColorSelectPopup* m_popup = nullptr;
    CCNode* m_layer = nullptr;
    CCMenu* m_menu = nullptr;
    std::vector<CCMenuItem*> m_orig; // [0]=P-Col1 [1]=P-Col2 [2]=Light BG [3]=Default
    std::map<int, ColorAction*> m_actions;
    std::vector<Ref<CCNode>> m_pageNodes;
    std::vector<Ref<CCNode>> m_recentNodes;
    CCLabelBMFont* m_pageLabel = nullptr;
    CCRect m_panel;
    float m_cell = 18.f;
    float m_startX = 0.f;
    float m_topY = 0.f;
    int m_page = 0;
    int m_pages = (MAX_CHANNEL + PER_PAGE - 1) / PER_PAGE;
    int m_selected = 0;

    CCPoint colCenter(float colOffset, int col, float rowOffset, int row) {
        return {
            m_startX + (colOffset + col + .5f) * m_cell,
            m_topY - .9f * m_cell - (rowOffset + row + .5f) * m_cell
        };
    }

    void place(CCNode* node, CCPoint layerPos) {
        node->setPosition(m_menu->convertToNodeSpace(m_layer->convertToWorldSpace(layerPos)));
    }

    CCNode* makeSwatch(int id, std::string const& label, ccColor3B fallback) {
        float size = m_cell * .9f;
        CCNode* sw = nullptr;
        auto it = m_actions.find(id);
        if (it != m_actions.end()) {
            auto spr = ColorChannelSprite::create();
            spr->updateValues(it->second);
            spr->setScale(size / std::max(1.f, spr->getContentSize().width));
            sw = spr;
        } else {
            auto spr = CCScale9Sprite::create("square02_001.png");
            spr->setContentSize({ size, size });
            spr->setColor(fallback);
            sw = spr;
        }
        if (!label.empty()) {
            auto lbl = CCLabelBMFont::create(label.c_str(), "bigFont.fnt");
            lbl->limitLabelWidth(size * .8f, .4f, .1f);
            lbl->setPosition(sw->getContentSize() / 2);
            if (sw->getScale() != 0.f) lbl->setScale(lbl->getScale() / sw->getScale() * (sw->getScale() < 1.f ? 1.f : 1.f));
            sw->addChild(lbl, 10);
        }
        return sw;
    }

    CCMenuItemSpriteExtra* makeButton(CCNode* spr, int tag, SEL_MenuHandler cb) {
        auto btn = CCMenuItemSpriteExtra::create(spr, nullptr, this, cb);
        btn->setTag(tag);
        return btn;
    }

    void addFrame(CCNode* btn) {
        auto frame = CCScale9Sprite::create("GJ_square07.png");
        float s = m_cell * 1.0f;
        frame->setContentSize({ s, s });
        frame->setPosition(btn->getContentSize() / 2);
        btn->addChild(frame, 20);
    }

    void buildSpecials() {
        auto gm = GameManager::get();
        for (int i = 0; i < 15; i++) {
            int col = i % 3;
            int row = i / 3;
            auto def = SPECIALS[i];
            ccColor3B fb { 90, 90, 90 };
            if (def.id == -1) fb = { 160, 90, 50 };
            if (def.id == -2) fb = gm->colorForIdx(gm->getPlayerColor());
            if (def.id == -3) fb = gm->colorForIdx(gm->getPlayerColor2());
            if (def.id == -4) fb = { 170, 150, 140 };
            if (def.id == 1010) fb = { 0, 0, 0 };
            if (def.id == 1011) fb = { 255, 255, 255 };
            auto sw = makeSwatch(def.id, def.label, fb);
            auto btn = makeButton(sw, def.id, menu_selector(ColorPanel::onSpecial));
            if (def.id >= 1000 && def.id == m_selected) addFrame(btn);
            place(btn, colCenter(0.f, col, 0.f, row));
            m_menu->addChild(btn);
        }
    }

    void buildPage() {
        for (auto& n : m_pageNodes) n->removeFromParent();
        m_pageNodes.clear();
        for (int j = 0; j < PER_PAGE; j++) {
            int id = m_page * PER_PAGE + j + 1;
            if (id > MAX_CHANNEL) break;
            auto sw = makeSwatch(id, std::to_string(id), { 90, 90, 90 });
            auto btn = makeButton(sw, id, menu_selector(ColorPanel::onPick));
            if (id == m_selected) addFrame(btn);
            place(btn, colCenter(3.5f, j % COLS, .5f, j / COLS));
            m_menu->addChild(btn);
            m_pageNodes.push_back(btn);
        }
        if (m_pageLabel) {
            m_pageLabel->setString(fmt::format("Page {} / {}", m_page + 1, m_pages).c_str());
        }
    }

    void buildRecent() {
        for (auto& n : m_recentNodes) n->removeFromParent();
        m_recentNodes.clear();
        auto recent = Mod::get()->getSavedValue<std::vector<int>>("recent");
        for (int k = 0; k < MAX_RECENT && k < static_cast<int>(recent.size()); k++) {
            int id = recent[k];
            auto sw = makeSwatch(id, id >= 1000 ? "" : std::to_string(id), { 90, 90, 90 });
            auto btn = makeButton(sw, id, menu_selector(ColorPanel::onSpecial));
            place(btn, colCenter(3.5f + COLS + .5f, k % 3, .5f, k / 3));
            m_menu->addChild(btn);
            m_recentNodes.push_back(btn);
        }
    }

    void addLabel(const char* text, CCPoint p, float width) {
        auto lbl = CCLabelBMFont::create(text, "goldFont.fnt");
        lbl->limitLabelWidth(width, .5f, .1f);
        lbl->setPosition(p);
        this->addChild(lbl, 2);
    }

    void pushRecent(int id) {
        auto recent = Mod::get()->getSavedValue<std::vector<int>>("recent");
        recent.erase(std::remove(recent.begin(), recent.end(), id), recent.end());
        recent.insert(recent.begin(), id);
        if (static_cast<int>(recent.size()) > MAX_RECENT) recent.resize(MAX_RECENT);
        Mod::get()->setSavedValue("recent", recent);
    }

    void onPage(CCObject* sender) {
        int dir = static_cast<CCNode*>(sender)->getTag();
        m_page = (m_page + dir + m_pages) % m_pages;
        this->buildPage();
    }

    void onPick(CCObject* sender) {
        auto node = static_cast<CCNode*>(sender);
        m_selected = node->getTag();
        m_popup->onUpdateCustomColor(node); // ваніль читає tag = номер каналу
        this->pushRecent(m_selected);
        this->buildPage();
        this->buildRecent();
    }

    void onSpecial(CCObject* sender) {
        auto node = static_cast<CCNode*>(sender);
        int id = node->getTag();
        if (id < 0) {
            int idx = (id == -1) ? 3 : (id == -2) ? 0 : (id == -3) ? 1 : 2;
            if (idx < static_cast<int>(m_orig.size())) m_orig[idx]->activate();
            return;
        }
        m_selected = id;
        m_popup->onUpdateCustomColor(node);
        if (id < 1000) this->pushRecent(id);
        this->buildPage();
        this->buildRecent();
    }

    bool init(ColorSelectPopup* popup, std::vector<CCMenuItem*> orig, CCRect bg) {
        if (!CCNode::init()) return false;
        m_popup = popup;
        m_layer = popup->m_mainLayer;
        m_menu = popup->m_buttonMenu;
        m_orig = std::move(orig);
        m_selected = popup->m_colorID;
        if (m_selected > 0) m_page = std::clamp((m_selected - 1) / PER_PAGE, 0, m_pages - 1);

        if (auto editor = LevelEditorLayer::get()) {
            if (editor->m_effectManager) {
                for (auto act : CCArrayExt<ColorAction*>(editor->m_effectManager->getAllColorActions())) {
                    m_actions[act->m_colorID] = act;
                }
            }
        }

        // темна вставка як на фото
        float W = bg.size.width, H = bg.size.height;
        m_topY = bg.getMaxY() - H * .21f;
        float Hp = H * .57f;
        float Wp = W * .93f;
        float cx = bg.getMidX();
        m_panel = CCRect(cx - Wp / 2, m_topY - Hp, Wp, Hp);
        m_cell = std::min(Wp / 12.4f, Hp / 5.95f);
        m_startX = cx - 6.f * m_cell;

        auto panel = CCScale9Sprite::create("square02_001.png");
        panel->setContentSize(m_panel.size);
        panel->setColor({ 0, 0, 0 });
        panel->setOpacity(95);
        panel->setPosition({ m_panel.getMidX(), m_panel.getMidY() });
        this->addChild(panel, 1);

        float hy = m_topY - .45f * m_cell;
        addLabel("Special", { m_startX + 1.5f * m_cell, hy }, m_cell * 3.f);
        addLabel("Recent", { m_startX + (3.5f + COLS + .5f + 1.5f) * m_cell, hy }, m_cell * 3.f);

        m_pageLabel = CCLabelBMFont::create("", "goldFont.fnt");
        m_pageLabel->setScale(.4f);
        m_pageLabel->setPosition({ m_startX + (3.5f + COLS / 2.f) * m_cell, hy });
        this->addChild(m_pageLabel, 2);

        for (int dir : { -1, 1 }) {
            auto spr = CCSprite::createWithSpriteFrameName("GJ_arrow_03_001.png");
            spr->setScale(m_cell * .8f / std::max(1.f, spr->getContentSize().width));
            spr->setFlipX(dir < 0);
            auto btn = makeButton(spr, dir, menu_selector(ColorPanel::onPage));
            float x = m_startX + (3.5f + COLS / 2.f + dir * (COLS / 2.f - .3f)) * m_cell;
            place(btn, { x, hy });
            m_menu->addChild(btn);
        }

        this->buildSpecials();
        this->buildPage();
        this->buildRecent();
        return true;
    }

public:
    static ColorPanel* create(ColorSelectPopup* popup, std::vector<CCMenuItem*> orig, CCRect bg) {
        auto ret = new ColorPanel();
        if (ret->init(popup, std::move(orig), bg)) {
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
        if (!m_buttonMenu || !m_mainLayer) return true;

        auto bgNode = m_mainLayer->getChildByType<CCScale9Sprite>(0);
        if (!bgNode) return true;
        CCRect bg = bgNode->boundingBox();
        if (bg.size.height < 10.f) return true;

        // знаходимо ванільні кнопки каналів по позиції (від 24% до 78% висоти вікна)
        std::vector<CCMenuItem*> region;
        for (auto child : CCArrayExt<CCNode*>(m_buttonMenu->getChildren())) {
            auto item = typeinfo_cast<CCMenuItem*>(child);
            if (!item) continue;
            auto p = m_mainLayer->convertToNodeSpace(
                m_buttonMenu->convertToWorldSpace(item->getPosition())
            );
            float rel = (bg.getMaxY() - p.y) / bg.size.height;
            if (rel > .24f && rel < .78f) region.push_back(item);
        }
        if (region.size() < 10) return true; // це не те меню — нічого не чіпаємо

        // верхній ряд (P-Col 1, P-Col 2, Light BG, Default) — запам'ятовуємо
        std::vector<CCMenuItem*> topRow;
        for (auto item : region) {
            auto p = m_mainLayer->convertToNodeSpace(
                m_buttonMenu->convertToWorldSpace(item->getPosition())
            );
            float rel = (bg.getMaxY() - p.y) / bg.size.height;
            if (rel < .42f) topRow.push_back(item);
        }
        std::sort(topRow.begin(), topRow.end(), [](CCMenuItem* a, CCMenuItem* b) {
            return a->getPositionX() < b->getPositionX();
        });

        for (auto item : region) {
            item->setVisible(false);
            item->setEnabled(false);
        }

        auto panel = ColorPanel::create(this, topRow, bg);
        if (!panel) return true;
        int z = bgNode->getZOrder() + 1;
        m_mainLayer->addChild(panel, z);
        if (m_buttonMenu->getZOrder() <= z) m_buttonMenu->setZOrder(z + 1);
        return true;
    }
};
