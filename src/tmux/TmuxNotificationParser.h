#ifndef TMUX__TMUXNOTIFICATIONPARSER_H
#define TMUX__TMUXNOTIFICATIONPARSER_H

#include "konsoleprivate_export.h"
// #include "TmuxServerManager.h"
#include "TmuxNotification.h"
#include "TmuxNotificationParseState.h"
#include <QSharedPointer>

namespace Konsole
{

struct TmuxServerManager;

struct KONSOLEPRIVATE_EXPORT TmuxNullNotification : TmuxNotification {
    TmuxNullNotification()
        : TmuxNotification("null")
    {
    }
    virtual void execute(TmuxServerManager &, TmuxNotificationParseState &) override
    {
    }
    virtual bool push_char(TmuxNotificationParseState &, uint) override
    {
        return true;
    }
    virtual void reset() override
    {
    }
    virtual ~TmuxNullNotification()
    {
    }
};

template<std::derived_from<TmuxNotification>... Ts>
class TmuxParser
{
    struct Node {
        std::size_t start;
        std::size_t end;
        std::unique_ptr<Node> sibling;
        std::unique_ptr<Node> child;
        TmuxNotification *data;
        bool hasData;
    };

    static void debugPrintHelper(QDebug dbg, const Node *n, int indent)
    {
        static QString indent_str("    ");
        if (!n)
            return;
        QDebug dbg2 = dbg.nospace();
        Node *child = n->child.get();
        for (int i = 0; i < indent; ++i)
            dbg2 << indent_str;
        dbg2 << "{" << n->start << ", " << n->end << ", \"";
        for (size_t i = n->start; i < n->end; ++i)
            dbg2 << n->data->name()[i];
        dbg2 << "\", " << n->data;
        dbg2 << ", " << n << ", " << n->child.get() << ", " << n->sibling.get();
        dbg2 << "}";
        while (child) {
            // std::cout << "here\n";
            debugPrintHelper(dbg, child, indent + 1);
            child = child->sibling.get();
        }
    }

    friend QDebug operator<<(QDebug dbg, const TmuxParser &p)
    {
        QDebugStateSaver saver(dbg);
        debugPrintHelper(dbg, p.m_root.get(), 0);
        return dbg;
    }

public:
    TmuxParser()
        : m_root{std::make_unique<Node>()}
        , m_parsers{Ts{}...}
    {
        make_trie(std::index_sequence_for<Ts...>{});
    }

    bool push_char(uint cc)
    {
        if (m_state.mode == TmuxNotificationParseState::Mode::ParseCommandName) {
            if (cc > 255) {
                qDebug() << "non-ascii character found parsing notification type:" << cc;
                return false;
            } else if (cc == ' ') {
                m_state.mode = TmuxNotificationParseState::Mode::ParseCommandArgs;
                return m_node->data->name()[m_pos] == '\0' && m_node->hasData;
            } else {
                if (m_node->end == m_pos) {
                    Node *next = m_node->child.get();
                    while (next && next->data->name()[m_pos] != cc) {
                        next = next->sibling;
                    }
                    if (!next) {
                        qDebug() << "unable to find notification child. Pos: " << m_pos << "Node:" << m_node->data->name();
                        qDebug() << *this;
                        return false;
                    }
                    m_node = next;
                    m_pos++;
                    return true;
                } else {
                    return m_node->data->name()[m_pos++] == cc;
                }
            }
        } else {
            if (m_state.
            return m_node->data->push_char(cc);
        }
    }

private:
    template<std::size_t... Is>
    void make_trie(std::index_sequence<Is...>)
    {
        (insert(&std::get<Is>(m_parsers)), ...);
    }
    void insert(TmuxNotification *t)
    {
        Node *current = m_root.get();

        const char *t_str = t->name();

        std::size_t i = 0;
        while (t_str[i] != '\0') {
            if (!current->child) {
                // No child nodes

                std::size_t j = i;
                while (t_str[j] != '\0')
                    j++;
                current->child = std::make_unique<Node>(i, j, nullptr, nullptr, t, true);
                return;
            } else {
                const char *next_str = current->child->data->name();
                if (t_str[i] < next_str[i]) {
                    // Insert as child head

                    std::size_t j = i;
                    while (t_str[j] != '\0')
                        j++;
                    std::unique_ptr<Node> tmp = std::move(current->child);
                    current->child = std::make_unique<Node>(i, j, std::move(tmp), nullptr, t, true);
                    return;
                } else {
                    Node *next = current->child.get();
                    const char *sibling_str;
                    while (next->sibling) {
                        sibling_str = next->sibling->data->name();
                        if (sibling_str[i] > t_str[i])
                            break;
                        next = next->sibling.get();
                    }
                    next_str = next->data->name();
                    if (next_str[i] == t_str[i]) {
                        while (i < next->end && t_str[i] != '\0' && next_str[i] == t_str[i])
                            i++;
                        if (i == next->end) {
                            current = next;
                            next_str = next->data->name();
                            continue;
                        } else { // At substring of existing node
                            // Split existing node into internal + leaf
                            std::size_t old_end = next->end;
                            bool old_isWord = next->isWord;
                            TmuxNotification *old_data = next->data;
                            next->end = i;
                            next->hasData = false;
                            std::size_t j = i;
                            while (t_str[j] != '\0')
                                j++;
                            if (next_str[i] < t_str[i]) {
                                next->child = std::make_unique<Node>(i, old_end, nullptr, nullptr, old_data, old_isWord);
                                next->child->sibling = std::make_unique<Node>(i, j, nullptr, nullptr, t, true);
                            } else {
                                next->child = std::make_unique<Node>(i, j, nullptr, nullptr, t, true);
                                next->child->sibling = std::make_unique<Node>(i, old_end, nullptr, nullptr, old_data, old_isWord);
                            }
                            return;
                        }
                    } else {
                        // child sibling
                        std::size_t j = i;
                        while (t_str[j] != '\0')
                            j++;
                        std::unique_ptr<Node> tmp = std::move(next->sibling);
                        next->sibling = std::make_unique<Node>(i, j, std::move(tmp), nullptr, t, true);
                        return;
                    }
                }
            }
        }
        // End of key
        if (t_str[i] == '\0') {
            current->hasData = true;
            current->data = t;
        }
    }

    std::unique_ptr<Node> m_root;
    std::tuple<Ts...> m_parsers;
    Node *m_node;
    qsizetype m_pos;
    TmuxNotificationParseState m_state;
};

}

#endif
