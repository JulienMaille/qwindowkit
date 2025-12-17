// Copyright (C) 2021-2023 wangwenx190 (Yuhang Zhao)
// Copyright (C) 2023-2024 Stdware Collections (https://www.github.com/stdware)
// SPDX-License-Identifier: Apache-2.0

#include "qtwindowcontext_p.h"

#include <QtCore/QDebug>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#  include <QtGui/private/qguiapplication_p.h>
#endif
#include <QtGui/qpa/qplatformwindow.h>

#include "qwkglobal_p.h"
#include "systemwindow_p.h"

#ifdef Q_OS_WINDOWS
#  include "shared/qwkwindowsextra_p.h"
#  include <windows.h>
#  include <windowsx.h>
#endif

namespace QWK {

    static constexpr const quint8 kDefaultResizeBorderThickness = 8;

#ifdef Q_OS_WINDOWS
    static bool showSystemMenu_sys(HWND hWnd, const POINT &pos, const bool selectFirstEntry,
                                   const bool fixedSize) {
        HMENU hMenu = ::GetSystemMenu(hWnd, FALSE);
        if (!hMenu) {
            return true;
        }

        const auto windowStyles = ::GetWindowLongPtrW(hWnd, GWL_STYLE);
        const bool allowMaximize = windowStyles & WS_MAXIMIZEBOX;
        const bool allowMinimize = windowStyles & WS_MINIMIZEBOX;

        // IsMaximized/IsFullScreen helpers needed?
        // Basic check for max/full
        const bool max = ::IsZoomed(hWnd);
        // Full screen check omitted for brevity, usually max check is enough for basic menu

        ::EnableMenuItem(hMenu, SC_CLOSE, (MF_BYCOMMAND | MFS_ENABLED));
        ::EnableMenuItem(
            hMenu, SC_MAXIMIZE,
            (MF_BYCOMMAND |
             ((max || fixedSize || !allowMaximize) ? MFS_DISABLED : MFS_ENABLED)));
        ::EnableMenuItem(
            hMenu, SC_RESTORE,
            (MF_BYCOMMAND |
             ((max && !fixedSize && allowMaximize) ? MFS_ENABLED : MFS_DISABLED)));
        ::HiliteMenuItem(hWnd, hMenu, SC_RESTORE,
                         (MF_BYCOMMAND | (selectFirstEntry ? MFS_HILITE : MFS_UNHILITE)));
        ::EnableMenuItem(hMenu, SC_MINIMIZE,
                         (MF_BYCOMMAND | (allowMinimize ? MFS_ENABLED : MFS_DISABLED)));
        ::EnableMenuItem(hMenu, SC_SIZE,
                         (MF_BYCOMMAND | ((max || fixedSize) ? MFS_DISABLED : MFS_ENABLED)));
        ::EnableMenuItem(hMenu, SC_MOVE, (MF_BYCOMMAND | (max ? MFS_DISABLED : MFS_ENABLED)));

        UINT defaultItemId = UINT_MAX;
        // Simple check for Win11 not strictly required, generic behavior is fine
        if (max) {
            defaultItemId = SC_RESTORE;
        } else {
            defaultItemId = SC_MAXIMIZE;
        }
        if (defaultItemId == UINT_MAX) {
            defaultItemId = SC_CLOSE;
        }
        ::SetMenuDefaultItem(hMenu, defaultItemId, FALSE);

        const auto result = ::TrackPopupMenu(
            hMenu,
            (TPM_RETURNCMD | (QGuiApplication::isRightToLeft() ? TPM_RIGHTALIGN : TPM_LEFTALIGN) |
             TPM_RIGHTBUTTON),
            pos.x, pos.y, 0, hWnd, nullptr);

        ::HiliteMenuItem(hWnd, hMenu, SC_RESTORE, (MF_BYCOMMAND | MFS_UNHILITE));

        if (!result) {
            return false;
        }

        ::PostMessageW(hWnd, WM_SYSCOMMAND, result, 0);
        return true;
    }

    struct Win32QtContextData {
        WNDPROC originalWindowProc = nullptr;
        QtWindowContext *context = nullptr;
    };

    static QHash<HWND, Win32QtContextData> g_qtContextHash;

    static LRESULT CALLBACK SystemMenuHookWindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        if (!g_qtContextHash.contains(hWnd)) {
            return ::DefWindowProcW(hWnd, uMsg, wParam, lParam);
        }
        const auto &data = g_qtContextHash[hWnd];

        bool shouldShowSystemMenu = false;
        bool broughtByKeyboard = false;
        POINT nativeGlobalPos = {};

        switch (uMsg) {
            case WM_RBUTTONUP: {
                POINT nativeLocalPos = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                QPoint qtScenePos = QHighDpi::fromNativeLocalPosition(point2qpoint(nativeLocalPos),
                                                                      data.context->window());
                if (data.context->isInTitleBarDraggableArea(qtScenePos)) {
                    POINT pos = nativeLocalPos;
                    ::ClientToScreen(hWnd, &pos);
                    shouldShowSystemMenu = true;
                    nativeGlobalPos = pos;
                }
                break;
            }
            case WM_NCRBUTTONUP: {
                if (wParam == HTCAPTION) {
                    shouldShowSystemMenu = true;
                    nativeGlobalPos = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                }
                break;
            }
            case WM_SYSCOMMAND: {
                const WPARAM filteredWParam = (wParam & 0xFFF0);
                if ((filteredWParam == SC_KEYMENU) && (lParam == VK_SPACE)) {
                    shouldShowSystemMenu = true;
                    broughtByKeyboard = true;
                    // Get position from window
                    RECT windowPos = {};
                    ::GetWindowRect(hWnd, &windowPos);
                    // Simple offset
                    nativeGlobalPos = {windowPos.left, windowPos.top + 30}; // Fallback
                }
                break;
            }
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN: {
                const bool altPressed = ((wParam == VK_MENU) || (::GetKeyState(VK_MENU) < 0));
                const bool spacePressed = ((wParam == VK_SPACE) || (::GetKeyState(VK_SPACE) < 0));
                if (altPressed && spacePressed) {
                    shouldShowSystemMenu = true;
                    broughtByKeyboard = true;
                    RECT windowPos = {};
                    ::GetWindowRect(hWnd, &windowPos);
                    nativeGlobalPos = {windowPos.left, windowPos.top + 30};
                }
                break;
            }
        }

        if (shouldShowSystemMenu) {
            showSystemMenu_sys(hWnd, nativeGlobalPos, broughtByKeyboard, data.context->isHostSizeFixed());
            return 0;
        }

        if (data.originalWindowProc) {
            return ::CallWindowProcW(data.originalWindowProc, hWnd, uMsg, wParam, lParam);
        }
        return ::DefWindowProcW(hWnd, uMsg, wParam, lParam);
    }

    static void installSystemMenuHook(HWND hwnd, QtWindowContext *ctx) {
        if (g_qtContextHash.contains(hwnd)) return;

        Win32QtContextData data;
        data.context = ctx;
        data.originalWindowProc = reinterpret_cast<WNDPROC>(::GetWindowLongPtrW(hwnd, GWLP_WNDPROC));

        if (data.originalWindowProc) {
            ::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(SystemMenuHookWindowProc));
            g_qtContextHash.insert(hwnd, data);
        }
    }

    static void uninstallSystemMenuHook(HWND hwnd) {
        if (!g_qtContextHash.contains(hwnd)) return;

        const auto &data = g_qtContextHash[hwnd];
        if (data.originalWindowProc) {
            ::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(data.originalWindowProc));
        }
        g_qtContextHash.remove(hwnd);
    }
#endif

    static Qt::CursorShape calculateCursorShape(const QWindow *window, const QPoint &pos) {
#ifdef Q_OS_MACOS
        Q_UNUSED(window);
        Q_UNUSED(pos);
        return Qt::ArrowCursor;
#else
        Q_ASSERT(window);
        if (!window) {
            return Qt::ArrowCursor;
        }
        if (window->visibility() != QWindow::Windowed) {
            return Qt::ArrowCursor;
        }
        const int x = pos.x();
        const int y = pos.y();
        const int w = window->width();
        const int h = window->height();
        if (((x < kDefaultResizeBorderThickness) && (y < kDefaultResizeBorderThickness)) ||
            ((x >= (w - kDefaultResizeBorderThickness)) &&
             (y >= (h - kDefaultResizeBorderThickness)))) {
            return Qt::SizeFDiagCursor;
        }
        if (((x >= (w - kDefaultResizeBorderThickness)) && (y < kDefaultResizeBorderThickness)) ||
            ((x < kDefaultResizeBorderThickness) && (y >= (h - kDefaultResizeBorderThickness)))) {
            return Qt::SizeBDiagCursor;
        }
        if ((x < kDefaultResizeBorderThickness) || (x >= (w - kDefaultResizeBorderThickness))) {
            return Qt::SizeHorCursor;
        }
        if ((y < kDefaultResizeBorderThickness) || (y >= (h - kDefaultResizeBorderThickness))) {
            return Qt::SizeVerCursor;
        }
        return Qt::ArrowCursor;
#endif
    }

    static inline Qt::Edges calculateWindowEdges(const QWindow *window, const QPoint &pos) {
#ifdef Q_OS_MACOS
        Q_UNUSED(window);
        Q_UNUSED(pos);
        return {};
#else
        Q_ASSERT(window);
        if (!window) {
            return {};
        }
        if (window->visibility() != QWindow::Windowed) {
            return {};
        }
        Qt::Edges edges = {};
        const int x = pos.x();
        const int y = pos.y();
        if (x < kDefaultResizeBorderThickness) {
            edges |= Qt::LeftEdge;
        }
        if (x >= (window->width() - kDefaultResizeBorderThickness)) {
            edges |= Qt::RightEdge;
        }
        if (y < kDefaultResizeBorderThickness) {
            edges |= Qt::TopEdge;
        }
        if (y >= (window->height() - kDefaultResizeBorderThickness)) {
            edges |= Qt::BottomEdge;
        }
        return edges;
#endif
    }

    class QtWindowEventFilter : public SharedEventFilter {
    public:
        explicit QtWindowEventFilter(AbstractWindowContext *context);
        ~QtWindowEventFilter() override;

        enum WindowStatus {
            Idle,
            WaitingRelease,
            PreparingMove,
            Moving,
            Resizing,
        };

    protected:
        bool sharedEventFilter(QObject *object, QEvent *event) override;

    private:
        AbstractWindowContext *m_context;
        bool m_cursorShapeChanged;
        WindowStatus m_windowStatus;
    };

    QtWindowEventFilter::QtWindowEventFilter(AbstractWindowContext *context)
        : m_context(context), m_cursorShapeChanged(false), m_windowStatus(Idle) {
        m_context->installSharedEventFilter(this);
    }

    QtWindowEventFilter::~QtWindowEventFilter() = default;

    bool QtWindowEventFilter::sharedEventFilter(QObject *obj, QEvent *event) {
        Q_UNUSED(obj)

        auto type = event->type();
        if (type < QEvent::MouseButtonPress || type > QEvent::MouseMove) {
            return false;
        }
        auto host = m_context->host();
        auto window = m_context->window();
        auto delegate = m_context->delegate();
        auto me = static_cast<const QMouseEvent *>(event);
        bool fixedSize = m_context->isHostSizeFixed();

        QPoint scenePos = getMouseEventScenePos(me);
        QPoint globalPos = getMouseEventGlobalPos(me);

        bool inTitleBar = m_context->isInTitleBarDraggableArea(scenePos);
        switch (type) {
            case QEvent::MouseButtonPress: {
                switch (me->button()) {
                    case Qt::LeftButton: {
                        if (!fixedSize) {
                            Qt::Edges edges = calculateWindowEdges(window, scenePos);
                            if (edges != Qt::Edges()) {
                                m_windowStatus = Resizing;
                                startSystemResize(window, edges);
                                event->accept();
                                return true;
                            }
                        }
                        if (inTitleBar) {
                            // If we call startSystemMove() now but release the mouse without actual
                            // movement, there will be no MouseReleaseEvent, so we defer it when the
                            // mouse is actually moving for the first time
                            m_windowStatus = PreparingMove;
                            event->accept();
                            return true;
                        }
                        break;
                    }
                    case Qt::RightButton: {
                        if (inTitleBar) {
                            m_context->showSystemMenu(globalPos);
                        }
                        break;
                    }
                    default:
                        break;
                }
                m_windowStatus = WaitingRelease;
                break;
            }

            case QEvent::MouseButtonRelease: {
                switch (m_windowStatus) {
                    case PreparingMove:
                    case Moving:
                    case Resizing: {
                        m_windowStatus = Idle;
                        event->accept();
                        return true;
                    }
                    case WaitingRelease: {
                        m_windowStatus = Idle;
                        break;
                    }
                    default: {
                        if (inTitleBar) {
                            event->accept();
                            return true;
                        }
                        break;
                    }
                }
                break;
            }

            case QEvent::MouseMove: {
                switch (m_windowStatus) {
                    case Moving: {
                        return true;
                    }
                    case PreparingMove: {
                        m_windowStatus = Moving;
                        startSystemMove(window);
                        event->accept();
                        return true;
                    }
                    case Idle: {
                        if (!fixedSize) {
                            const Qt::CursorShape shape = calculateCursorShape(window, scenePos);
                            if (shape == Qt::ArrowCursor) {
                                if (m_cursorShapeChanged) {
                                    delegate->restoreCursorShape(host);
                                    m_cursorShapeChanged = false;
                                }
                            } else {
                                delegate->setCursorShape(host, shape);
                                m_cursorShapeChanged = true;
                            }
                        }
                        break;
                    }
                    default:
                        break;
                }
                break;
            }

            case QEvent::MouseButtonDblClick: {
                if (me->button() == Qt::LeftButton && inTitleBar && !fixedSize) {
                    Qt::WindowFlags windowFlags = delegate->getWindowFlags(host);
                    Qt::WindowStates windowState = delegate->getWindowState(host);
                    if ((windowFlags & Qt::WindowMaximizeButtonHint) &&
                        !(windowState & Qt::WindowFullScreen)) {
                        if (windowState & Qt::WindowMaximized) {
                            delegate->setWindowState(host, windowState & ~Qt::WindowMaximized);
                        } else {
                            delegate->setWindowState(host, windowState | Qt::WindowMaximized);
                        }
                        event->accept();
                        return true;
                    }
                }
                break;
            }

            default:
                break;
        }
        return false;
    }

    QtWindowContext::QtWindowContext() : AbstractWindowContext() {
        qtWindowEventFilter = std::make_unique<QtWindowEventFilter>(this);
    }

    QtWindowContext::~QtWindowContext() {
#ifdef Q_OS_WINDOWS
        if (m_windowId) {
            uninstallSystemMenuHook(reinterpret_cast<HWND>(m_windowId));
        }
#endif
    }

    QString QtWindowContext::key() const {
        return QStringLiteral("qt");
    }

    void QtWindowContext::virtual_hook(int id, void *data) {
#ifdef Q_OS_WINDOWS
        if (id == ShowSystemMenuHook && m_windowId) {
            const auto &pos = *static_cast<const QPoint *>(data);
            const auto hwnd = reinterpret_cast<HWND>(m_windowId);
            const QPoint nativeGlobalPos = QHighDpi::toNativeGlobalPosition(pos, m_windowHandle.data());
            showSystemMenu_sys(hwnd, qpoint2point(nativeGlobalPos), false, isHostSizeFixed());
            return;
        }
#endif
        AbstractWindowContext::virtual_hook(id, data);
    }

    void QtWindowContext::winIdChanged(WId winId, WId oldWinId) {
        if (m_windowHandle) {
            // Allocate new resources
            m_delegate->setWindowFlags(m_host,
                                   m_delegate->getWindowFlags(m_host) | Qt::FramelessWindowHint);
        } else {
             m_delegate->setWindowFlags(m_host, m_delegate->getWindowFlags(m_host) &
                                                   ~Qt::FramelessWindowHint);
        }

#ifdef Q_OS_WINDOWS
        if (oldWinId) {
            uninstallSystemMenuHook(reinterpret_cast<HWND>(oldWinId));
        }
        if (winId) {
            installSystemMenuHook(reinterpret_cast<HWND>(winId), this);
        }
#endif
    }

}
