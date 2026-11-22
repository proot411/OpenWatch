#pragma once
#include <QString>

// OpenWatch visual theme.
//
// One palette, one source of truth. The tokens below feed both the stylesheet
// and the hand-painted widgets (cell.h), so the chrome and the video grid can
// never drift apart into two different accent colours again.
//
// Surface ladder:  bg (deepest) -> surface (chrome) -> raised (controls) -> hover
// Recessed:        input (fields, viewports, video cells, tracks)
// Accent:          exactly one teal, for live / selected / focus.
// Red:             record and destruction only. Nothing else is coloured.
namespace theme {
inline constexpr const char *bg        = "#0D1216";
inline constexpr const char *surface   = "#141B22";
inline constexpr const char *raised    = "#1C252E";
inline constexpr const char *hover     = "#242F39";
inline constexpr const char *input     = "#0B1014";
inline constexpr const char *border    = "#27323B";
inline constexpr const char *edge      = "#36444F";
inline constexpr const char *text      = "#E7EEF3";
inline constexpr const char *text2     = "#9FB0BC";
inline constexpr const char *muted     = "#7C8D99";
inline constexpr const char *disabled  = "#5F6E79";
inline constexpr const char *accent    = "#35C9AE";
inline constexpr const char *accentDim = "#0F3A33";
inline constexpr const char *accentMid = "#1E9E88";
inline constexpr const char *onAccent  = "#04120F";
inline constexpr const char *danger    = "#FF394F";
inline constexpr const char *dangerDim = "#8C2031";
inline constexpr const char *onDanger  = "#FFD2D6";
// Painted video-cell surfaces.
inline constexpr const char *cellBg    = "#0B1014";
inline constexpr const char *cellStrip = "#151D24";
inline constexpr const char *cellText  = "#A9BAC6";

inline QString stylesheet(){
 static const struct Token{const char *name;const char *value;} tokens[]={
  {"__BG__",bg},{"__SURFACE__",surface},{"__RAISED__",raised},{"__HOVER__",hover},
  {"__INPUT__",input},{"__BORDER__",border},{"__EDGE__",edge},
  {"__TEXT__",text},{"__TEXT2__",text2},{"__MUTED__",muted},{"__DISABLED__",disabled},
  {"__ACCENT__",accent},{"__ACCENTDIM__",accentDim},{"__ACCENTMID__",accentMid},{"__ONACCENT__",onAccent},
  {"__DANGER__",danger},{"__DANGERDIM__",dangerDim},{"__ONDANGER__",onDanger},
  {"__CELLBG__",cellBg},{"__CELLSTRIP__",cellStrip},{"__CELLTEXT__",cellText},
 };
 QString css=QStringLiteral(R"CSS(
/* ---- Surfaces ---- */
QWidget{background:__BG__;color:__TEXT__;font-family:'Noto Sans';font-size:13px;}
QMainWindow{background:__BG__;}
QDialog{background:__BG__;}
QLabel{background:transparent;}

/* ---- Menu bar ---- */
QMenuBar{background:__SURFACE__;border-bottom:1px solid __BORDER__;padding:2px 8px;}
QMenuBar::item{padding:5px 10px;background:transparent;border-radius:4px;margin:1px 0;color:__TEXT2__;}
QMenuBar::item:selected{background:__HOVER__;color:__TEXT__;}
QMenuBar::item:pressed{background:__INPUT__;}
QMenuBar::item:disabled{color:__DISABLED__;}

/* ---- Menus ---- */
QMenu{background:__SURFACE__;border:1px solid __BORDER__;padding:4px;}
QMenu::item{padding:6px 24px 6px 14px;margin:1px 2px;border-radius:4px;color:__TEXT2__;}
QMenu::item:selected{background:__HOVER__;color:__TEXT__;}
QMenu::item:disabled{color:__DISABLED__;}
QMenu::item:checked{background:__ACCENTDIM__;color:__TEXT__;}
QMenu::separator{height:1px;background:__BORDER__;margin:5px 8px;}

/* ---- Toolbar ---- */
QToolBar{background:__SURFACE__;border:none;border-bottom:1px solid __BORDER__;padding:7px 10px;spacing:6px;}
QToolBar::separator{background:__BORDER__;width:1px;margin:4px 6px;}

/* ---- Buttons: borderless at rest, hairline on hover, accent on focus ---- */
QPushButton{background:__RAISED__;border:1px solid transparent;border-radius:5px;padding:7px 13px;color:__TEXT2__;}
QPushButton:hover{background:__HOVER__;border-color:__EDGE__;color:__TEXT__;}
QPushButton:pressed{background:__INPUT__;color:__TEXT__;}
QPushButton:focus{border-color:__ACCENT__;color:__TEXT__;}
QPushButton:disabled{background:__SURFACE__;border-color:transparent;color:__DISABLED__;}
QPushButton:default{background:__ACCENTMID__;border-color:__ACCENTMID__;color:__ONACCENT__;font-weight:600;}
QPushButton:default:hover{background:__ACCENT__;border-color:__ACCENT__;}
QPushButton#recordToggle:checked{background:__DANGERDIM__;border-color:__DANGER__;color:__ONDANGER__;}

/* ---- Text fields ---- */
QLineEdit,QSpinBox,QDoubleSpinBox{
 background:__INPUT__;border:1px solid __EDGE__;border-radius:5px;padding:6px 9px;color:__TEXT__;
 selection-background-color:__ACCENTDIM__;selection-color:__TEXT__;}
QLineEdit:focus,QSpinBox:focus,QDoubleSpinBox:focus{border-color:__ACCENT__;}
QLineEdit:disabled,QSpinBox:disabled,QDoubleSpinBox:disabled{background:__SURFACE__;color:__DISABLED__;border-color:__BORDER__;}

/* ---- Combo boxes ---- */
QComboBox{background:__INPUT__;border:1px solid __EDGE__;border-radius:5px;padding:6px 26px 6px 9px;color:__TEXT__;min-height:20px;}
QComboBox:hover{border-color:__MUTED__;}
QComboBox:focus{border-color:__ACCENT__;}
QComboBox:disabled{background:__SURFACE__;color:__DISABLED__;border-color:__BORDER__;}
QComboBox::down-arrow{subcontrol-origin:padding;subcontrol-position:center right;margin-right:5px;}
QComboBox QAbstractItemView{background:__SURFACE__;border:1px solid __BORDER__;selection-background-color:__ACCENTDIM__;selection-color:__TEXT__;color:__TEXT2__;outline:none;padding:3px;}
/* Menu buttons carry their own ▾ in the label; the native indicator lands on the
   border and gets clipped by the corner radius. */
QPushButton::menu-indicator{image:none;}

/* ---- Lists and trees ---- */
QListWidget,QTreeWidget,QTextEdit{
 background:__INPUT__;border:1px solid __EDGE__;border-radius:5px;padding:2px;color:__TEXT__;
 selection-background-color:__ACCENTDIM__;selection-color:__TEXT__;}
QListWidget:focus,QTreeWidget:focus,QTextEdit:focus{border-color:__ACCENT__;}
QTreeWidget::item,QListWidget::item{padding:5px 4px;border-radius:4px;color:__TEXT2__;}
QTreeWidget::item:selected,QListWidget::item:selected{background:__ACCENTDIM__;color:__TEXT__;}
QTreeWidget::item:hover,QListWidget::item:hover{background:__HOVER__;}

/* ---- Tabs: flat, accent underline ---- */
QTabWidget::pane{border:1px solid __BORDER__;background:__BG__;}
QTabBar::tab{background:transparent;color:__MUTED__;padding:8px 18px;margin-right:2px;border-bottom:2px solid transparent;}
QTabBar::tab:selected{color:__TEXT__;border-bottom-color:__ACCENT__;}
QTabBar::tab:hover{color:__TEXT2__;}

/* ---- Status bar ---- */
QStatusBar{background:__SURFACE__;border-top:1px solid __BORDER__;color:__TEXT2__;font-size:12px;}
QStatusBar::item{border:none;}

/* ---- Labels ---- */
QLabel#brand{padding:0 8px;font-size:15px;font-weight:600;color:__ACCENT__;}
QLabel#hint{color:__MUTED__;font-size:12px;}
/* Live audio readout: muted at rest, accent while streaming, red on device error. */
QLabel#audioState{color:__MUTED__;font-size:12px;}
QLabel#audioState[state="live"]{color:__ACCENT__;}
QLabel#audioState[state="error"]{color:__DANGER__;}
QLabel#emptyHint{color:__MUTED__;font-size:12px;background:__INPUT__;border:1px dashed __EDGE__;border-radius:6px;padding:16px;}

/* ---- Slider ---- */
QSlider::groove:horizontal{height:4px;background:__BORDER__;border-radius:2px;}
QSlider::sub-page:horizontal{background:__ACCENT__;border-radius:2px;}
QSlider::handle:horizontal{background:__TEXT2__;border:none;width:14px;height:14px;margin:-6px 0;border-radius:7px;}
QSlider::handle:horizontal:hover{background:__ACCENT__;}

/* ---- Checks and radios ---- */
QCheckBox::indicator,QRadioButton::indicator{width:16px;height:16px;border:1px solid __EDGE__;background:__INPUT__;}
QCheckBox::indicator{border-radius:3px;}
QCheckBox::indicator:checked{background:__ACCENT__;border-color:__ACCENT__;}
QCheckBox::indicator:hover{border-color:__ACCENT__;}
QRadioButton::indicator{border-radius:9px;}
QRadioButton::indicator:checked{background:__ACCENT__;border-color:__ACCENT__;}
QRadioButton::indicator:hover{border-color:__ACCENT__;}

/* ---- Scroll bars ---- */
QScrollBar:vertical{background:__INPUT__;width:10px;margin:0;border-radius:5px;}
QScrollBar::handle:vertical{background:__EDGE__;border-radius:5px;min-height:32px;}
QScrollBar::handle:vertical:hover{background:__MUTED__;}
QScrollBar:horizontal{background:__INPUT__;height:10px;margin:0;border-radius:5px;}
QScrollBar::handle:horizontal{background:__EDGE__;border-radius:5px;min-width:32px;}
QScrollBar::handle:horizontal:hover{background:__MUTED__;}
QScrollBar::add-line,QScrollBar::sub-line,QScrollBar::add-page,QScrollBar::sub-page{background:none;border:none;height:0;width:0;}
QScrollBar::corner{background:transparent;}

/* ---- Tool tips and message boxes ---- */
QToolTip{background:__RAISED__;color:__TEXT__;border:1px solid __EDGE__;padding:5px 8px;font-size:12px;}
QMessageBox{background:__SURFACE__;}
QMessageBox QLabel{color:__TEXT2__;}
)CSS");
 for(const auto &token:tokens)css.replace(QString::fromLatin1(token.name),QString::fromLatin1(token.value));
 return css;
}
}
