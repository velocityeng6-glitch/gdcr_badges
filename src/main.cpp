#include <Geode/Geode.hpp>

#include <Geode/modify/ProfilePage.hpp>
#include <Geode/modify/CommentCell.hpp>
#include <Geode/modify/LeaderboardsLayer.hpp>
#include <Geode/modify/MenuLayer.hpp>

#include <Geode/ui/Popup.hpp>

#include <Geode/binding/GJUserScore.hpp>
#include <Geode/binding/GJComment.hpp>
#include <Geode/binding/FLAlertLayer.hpp>
#include <Geode/binding/CCMenuItemSpriteExtra.hpp>

#include <Geode/utils/web.hpp>
#include <Geode/loader/Event.hpp>   // <-- для EventListener<WebTask>

#include "json.hpp"
#include <unordered_map>
#include <vector>
#include <string>
#include <cctype>
#include <chrono>
using namespace std::chrono_literals;

using namespace geode::prelude;
using json = nlohmann::json;

// gist json

static constexpr const char* BADGES_URL =
"https://gist.githubusercontent.com/velocityeng6-glitch/13127e4202035939726c73d570077e88/raw/badges.json";

enum class Kind { None, Yellow, Red, Owner };

static std::unordered_map<int, Kind> gRoles;

static Kind kindFromString(std::string s) {
    for (auto& c : s) c = (char)std::toupper((unsigned char)c);
    if (s == "OWNER")    return Kind::Owner;
    if (s == "HEAD")     return Kind::Red;
    if (s == "REVIEWER") return Kind::Yellow;
    return Kind::None;
}

static void applyBadgesJSON(const std::string& body) {
    auto j = json::parse(body, nullptr, /*allow_exceptions=*/false);
    if (j.is_discarded()) {
        return;
    }
    if (!j.contains("players") || !j["players"].is_object()) {
        return;
    }

    std::unordered_map<int, Kind> newMap;
    for (auto& [idStr, obj] : j["players"].items()) {
        int id = 0;
        try { id = std::stoi(idStr); }
        catch (...) { continue; }
        if (!obj.contains("roles") || !obj["roles"].is_array()) continue;

        Kind k = Kind::None;
        for (auto& r : obj["roles"]) {
            if (!r.is_string()) continue;
            k = kindFromString(r.get<std::string>());
            if (k != Kind::None) break;
        }
        if (k == Kind::None) continue;

        newMap[id] = k;
    }

    gRoles.swap(newMap);
}

// badges

static CCSprite* makeBadge(Kind k) {
    if (k == Kind::Owner)  return CCSprite::create("badge_owner.png"_spr);
    if (k == Kind::Red)    return CCSprite::create("badge_red.png"_spr);
    if (k == Kind::Yellow) return CCSprite::create("badge_yellow.png"_spr);
    return nullptr;
}

static void showBadgeInfo(Kind kind) {
    const char* title = "Reviewer";
    const char* desc =
        "<cg>Reviewers</c> are responsible for <cb>making reviews</c> and <cb>giving feedbacks</c> to creators. "
        "Feel free to reach out to such players for <co>any help related to levels.</c>";

    if (kind == Kind::Red) {
        title = "Head Reviewer";
        desc =
            "<cr>Head Reviewer</c> are responsible for <cb>making reviews</c> and <cb>giving feedbacks</c> to creators. "
            "Also they're pillars of the team and they have power to <cb>promote</c> and <cb>demote</c> <cg>new reviewers</c>. "
            "Feel free to reach out to such players for <co>any help related to levels.</c>";
    }
    else if (kind == Kind::Owner) {
        title = "Owner";
        desc =
            "<cy>Owner</c> of the project. Coordinates reviewers and decisions. "
            "Feel free to reach out regarding <cb>review process</c> and <cb>team matters</c>.";
    }

    FLAlertLayer::create(title, desc, "OK")->show();
}

// classification

static Kind classifyByIDs(int accountID, int userID) {
    if (auto it = gRoles.find(accountID); it != gRoles.end()) return it->second;
    if (auto it = gRoles.find(userID);    it != gRoles.end()) return it->second;
    return Kind::None;
}

static Kind classify(GJUserScore* p) { return p ? classifyByIDs(p->m_accountID, p->m_userID) : Kind::None; }
static Kind classify(GJComment* c) { return c ? classifyByIDs(c->m_accountID, c->m_userID) : Kind::None; }

// ui hooks

class $modify(MyMenuLayer, MenuLayer) {
public:
    struct Fields {
        EventListener<geode::utils::web::WebTask> listener;
        bool bound = false;
    };

    bool init() {
        if (!MenuLayer::init()) return false;

        if (!m_fields->bound) {
            m_fields->bound = true;

            m_fields->listener.bind([](geode::utils::web::WebTask::Event* e) {
                using namespace geode::utils::web;
                if (auto* res = e->getValue()) {
                    if (!res->ok()) {
                        return;
                    }
                    auto body = res->string().unwrapOr(std::string{});
                    if (body.empty()) {
                        return;
                    }
                    applyBadgesJSON(body);
                }
                else if (e->isCancelled()) {
                }
                });

            geode::utils::web::WebRequest req;
            req.userAgent("GDCR-Mod/1.0").timeout(5s);
            m_fields->listener.setFilter(req.get(BADGES_URL));
        }

		// button at the right side of the main menu
        auto sprite = CCSprite::create("button_menu.png"_spr);
        auto myButton = CCMenuItemSpriteExtra::create(sprite, this, menu_selector(MyMenuLayer::onMyButton));

        auto menu = typeinfo_cast<CCMenu*>(this->getChildByIDRecursive("right-side-menu"));
        if (!menu) return true;

        myButton->setID("gdcr-reviewers-button");
        menu->addChild(myButton, 999);
        menu->updateLayout();
        return true;
    }

    void onMyButton(CCObject*) {
        geode::createQuickPopup(
            "Who are Reviewers?",
            "<cl>Reviewers</c> are people who <cy>help keep the quality of levels high by providing feedback and reviews</c>. \n Join our <co>Discord server</c> if you want to become Reviewer or to get help with your level.",
            "Cancel", "Join",
            [](auto, bool btn2) {
                if (btn2) {
                    geode::utils::web::openLinkInBrowser("https://discord.gg/tmf5xtCX5y");
                }
            }

        );
    }
};

// progile hook
class $modify(GDCR_ProfilePage, ProfilePage) {
    void onBadgeInfo(CCObject * sender) {
        showBadgeInfo(static_cast<Kind>(typeinfo_cast<CCNode*>(sender)->getTag()));
    }
    void loadPageFromUserInfo(GJUserScore * profile) {
        ProfilePage::loadPageFromUserInfo(profile);

        auto kind = classify(profile);
        if (kind == Kind::None) return;

        auto menu = typeinfo_cast<CCMenu*>(this->getChildByIDRecursive("username-menu"));
        if (!menu) return;

        if (auto old = menu->getChildByID("gdcr-badge")) old->removeFromParent();

        auto spr = makeBadge(kind);
        if (!spr) return;
        spr->setAnchorPoint(ccp(0.5f, 0.5f));
        spr->setScale(0.75f);

        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GDCR_ProfilePage::onBadgeInfo));
        btn->setID("gdcr-badge");
        btn->setTag(static_cast<int>(kind));

        menu->addChild(btn);
        menu->updateLayout();
    }
};

// comment cell hook
class $modify(GDCR_CommentCell, CommentCell) {
    void onBadgeInfo(CCObject * sender) {
        showBadgeInfo(static_cast<Kind>(typeinfo_cast<CCNode*>(sender)->getTag()));
    }
    void loadFromComment(GJComment * c) {
        CommentCell::loadFromComment(c);

        auto kind = classify(c);
        if (kind == Kind::None) return;

        auto menu = typeinfo_cast<CCMenu*>(this->getChildByIDRecursive("username-menu"));
        if (!menu) return;

        if (auto old = menu->getChildByID("gdcr-badge")) old->removeFromParent();

        auto spr = makeBadge(kind);
        if (!spr) return;
        spr->setAnchorPoint(ccp(0.5f, 0.5f));
        spr->setScale(0.50f);

        auto btn = CCMenuItemSpriteExtra::create(spr, this, menu_selector(GDCR_CommentCell::onBadgeInfo));
        btn->setID("gdcr-badge");
        btn->setTag(static_cast<int>(kind));

        menu->addChild(btn);
        menu->updateLayout();
    }
};
