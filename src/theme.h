#pragma once
#include <QString>

// OpenWatch visual theme. Operational dark palette: neutral slate surfaces,
// green = live/selected, red = recording/danger, amber = warning.
namespace theme {
inline QString stylesheet(){
return QStringLiteral(R"CSS(
/* ---- Base ---- */
QWidget{background:#0F172A;color:#E2E8F0;font-family:'Noto Sans';font-size:13px;}
QMainWindow{background:#0F172A;}
QDialog{background:#0F172A;}

/* ---- Menu bar ---- */
QMenuBar{background:#111827;border-bottom:1px solid #334155;padding:2px 8px;}
QMenuBar::item{padding:6px 12px;background:transparent;border-radius:4px;margin:2px 0;}
QMenuBar::item:selected{background:#1E293B;color:#F8FAFC;}
QMenuBar::item:pressed{background:#0B1220;}
QMenuBar::item:disabled{color:#64748B;}

/* ---- Menus ---- */
QMenu{background:#111827;border:1px solid #334155;padding:6px;}
QMenu::item{padding:7px 26px 7px 16px;margin:1px 3px;border-radius:4px;}
QMenu::item:selected{background:#1E293B;color:#F8FAFC;}
QMenu::item:disabled{color:#64748B;}
QMenu::item:checked{background:#14532D;color:#F8FAFC;}
QMenu::separator{height:1px;background:#334155;margin:6px 8px;}

/* ---- Toolbar ---- */
QToolBar{background:#111827;border:none;border-bottom:1px solid #334155;padding:8px 10px;spacing:6px;}
QToolBar::separator{background:#334155;width:1px;margin:4px 6px;}

/* ---- Buttons ---- */
QPushButton{background:#1E293B;border:1px solid #334155;border-radius:6px;padding:7px 14px;color:#E2E8F0;}
QPushButton:hover{background:#273549;border-color:#475569;}
QPushButton:pressed{background:#0B1220;}
QPushButton:focus{border-color:#22C55E;}
QPushButton:disabled{background:#16202E;color:#64748B;border-color:#25303F;}
QPushButton:default{background:#166534;border-color:#22C55E;color:#F8FAFC;}
QPushButton:default:hover{background:#15803D;}
QPushButton#recordToggle:checked{background:#7F1D1D;border-color:#DC2626;color:#FECACA;}

/* ---- Inputs ---- */
QLineEdit,QListWidget,QTreeWidget,QTextEdit,QSpinBox,QDoubleSpinBox{
 background:#0B1220;border:1px solid #334155;border-radius:6px;padding:6px 8px;color:#E2E8F0;
 selection-background-color:#14532D;selection-color:#F8FAFC;}
QLineEdit:focus,QListWidget:focus,QTreeWidget:focus,QSpinBox:focus,QDoubleSpinBox:focus{border-color:#22C55E;}
QLineEdit:disabled,QSpinBox:disabled{background:#16202E;color:#64748B;border-color:#25303F;}
QLineEdit[placeholderText]{color:#94A3B8;}

/* ---- Combo boxes ---- */
QComboBox{background:#1E293B;border:1px solid #334155;border-radius:6px;padding:6px 10px;color:#E2E8F0;min-height:20px;}
QComboBox:hover{border-color:#475569;}
QComboBox:focus{border-color:#22C55E;}
QComboBox:disabled{background:#16202E;color:#64748B;border-color:#25303F;}
QComboBox QAbstractItemView{background:#111827;border:1px solid #334155;selection-background-color:#14532D;selection-color:#F8FAFC;color:#E2E8F0;outline:none;padding:4px;}

/* ---- Tree / list items ---- */
QTreeWidget::item,QListWidget::item{padding:6px 4px;border-radius:4px;}
QTreeWidget::item:selected,QListWidget::item:selected{background:#14532D;color:#F8FAFC;}
QTreeWidget::item:hover,QListWidget::item:hover{background:#16202E;}
QTreeWidget::item:focus{border:1px solid #22C55E;}

/* ---- Tabs ---- */
QTabWidget::pane{border:1px solid #334155;background:#0F172A;}
QTabBar::tab{background:#16202E;padding:9px 20px;border:1px solid #334155;border-bottom:none;margin-right:2px;color:#94A3B8;border-radius:6px 6px 0 0;}
QTabBar::tab:selected{background:#0F172A;color:#F8FAFC;border-top:2px solid #22C55E;}
QTabBar::tab:hover{color:#E2E8F0;}

/* ---- Status bar ---- */
QStatusBar{background:#111827;border-top:1px solid #334155;color:#94A3B8;}
QStatusBar::item{border:none;}

/* ---- Labels ---- */
QLabel#brand{background:transparent;border:none;padding:0 8px;font-size:18px;font-weight:700;color:#4ADE80;}
QLabel#sectionCaption{color:#94A3B8;font-size:11px;font-weight:600;padding:8px 2px 2px 2px;}
QLabel#hint,QLabel#emptyState{color:#94A3B8;font-size:12px;}

/* ---- Slider ---- */
QSlider::groove:horizontal{height:4px;background:#334155;border-radius:2px;}
QSlider::sub-page:horizontal{background:#22C55E;border-radius:2px;}
QSlider::handle:horizontal{background:#E2E8F0;border:1px solid #475569;width:14px;height:14px;margin:-6px 0;border-radius:7px;}
QSlider::handle:horizontal:hover{background:#4ADE80;}

/* ---- Checks & radios ---- */
QCheckBox::indicator,QRadioButton::indicator{width:16px;height:16px;border:1px solid #475569;background:#0B1220;}
QCheckBox::indicator{border-radius:3px;}
QCheckBox::indicator:checked{background:#22C55E;border-color:#22C55E;}
QRadioButton::indicator{border-radius:9px;}
QRadioButton::indicator:checked{background:#22C55E;border-color:#22C55E;}

/* ---- Scroll bars ---- */
QScrollBar:vertical{background:#0B1220;width:12px;margin:0;border-radius:6px;}
QScrollBar::handle:vertical{background:#334155;border-radius:6px;min-height:32px;}
QScrollBar::handle:vertical:hover{background:#475569;}
QScrollBar:horizontal{background:#0B1220;height:12px;margin:0;border-radius:6px;}
QScrollBar::handle:horizontal{background:#334155;border-radius:6px;min-width:32px;}
QScrollBar::handle:horizontal:hover{background:#475569;}
QScrollBar::add-line,QScrollBar::sub-line,QScrollBar::add-page,QScrollBar::sub-page{background:none;border:none;height:0;width:0;}
QScrollBar::corner{background:transparent;}

/* ---- Tool tips ---- */
QToolTip{background:#1E293B;color:#F8FAFC;border:1px solid #475569;padding:5px 8px;font-size:12px;}

/* ---- Message boxes ---- */
QMessageBox{background:#111827;}
QMessageBox QLabel{color:#E2E8F0;}
)CSS");
}
}
