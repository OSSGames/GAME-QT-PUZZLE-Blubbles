/*
    Blubbels
    Copyright (C) 2007-2010 Christian Pulvermacher

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include "game.h"
#include "main.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QToolBar>
#include <QTranslator>

#define MAEMO (defined Q_WS_HILDON || defined Q_WS_MAEMO_5)

//default settings
const bool USE_ANIMATIONS = true;
#if MAEMO
const bool USE_SOUND = false;
#else
const bool USE_SOUND = true;
#endif


MainWindow::MainWindow(QWidget *parent) :
	QMainWindow(parent),
	gamewidget(new GameWidget(this)),
	score_label(new QLabel(this))
{
	setWindowTitle("Blubbels");

	gamewidget->enableAnimations(settings.value("use_animation", USE_ANIMATIONS).toBool());
	gamewidget->enableSound(settings.value("use_sound", USE_SOUND).toBool());
	setCentralWidget(gamewidget);

#ifndef USE_SHARED_DIR
	setWindowIcon(QIcon("icons/64x64/blubbels.png"));
#else
	setWindowIcon(QIcon("/usr/share/icons/hicolor/64x64/apps/blubbels.png"));
#endif

	//construct actions
	QAction *new_game = new QAction(QIcon(":/icons/new.png"), tr("&New Game"), this);
	new_game->setShortcut(tr("Ctrl+N"));
	QAction *undo = new QAction(QIcon(":/icons/undo.png"), tr("&Undo"), this);
	undo->setShortcut(tr("Ctrl+Z"));
	undo->setEnabled(false);
	use_sound = new QAction(tr("&Use Sound"), this);
	use_sound->setCheckable(true);
	use_sound->setChecked(settings.value("use_sound", USE_SOUND).toBool());
	use_animation = new QAction(tr("&Animate Bubbles"), this);
	use_animation->setCheckable(true);
	use_animation->setChecked(settings.value("use_animation", USE_ANIMATIONS).toBool());

#if !MAEMO
	statusBar()->addPermanentWidget(score_label);

	//create toolbar
	QToolBar *toolbar = new QToolBar(tr("Show Toolbar"), this);
	toolbar->setObjectName("toolBar"); //used by saveState()
	toolbar->addAction(new_game);
	toolbar->addSeparator();
	toolbar->addAction(undo);
	addToolBar(toolbar);

	//create menu
	QMenu *game_menu = menuBar()->addMenu(tr("&Game"));
	game_menu->addAction(new_game);
	game_menu->addSeparator();
	game_menu->addAction(tr("&Statistics"), this, SLOT(showStatistics()), tr("Ctrl+S"));
	game_menu->addSeparator();
	game_menu->addAction(tr("&Quit"), this, SLOT(close()), tr("Ctrl+Q"));

	QMenu *edit_menu = menuBar()->addMenu(tr("&Edit"));
	edit_menu->addAction(undo);

	QMenu *settings_menu = menuBar()->addMenu(tr("&Settings"));
	settings_menu->addAction(use_sound);
	settings_menu->addAction(use_animation);
	settings_menu->addSeparator();
	settings_menu->addAction(toolbar->toggleViewAction());

	QMenu *help_menu = menuBar()->addMenu(tr("&Help"));
	help_menu->addAction(tr("&How to Play"), this, SLOT(help()), tr("F1"));
	help_menu->addSeparator();
	help_menu->addAction(tr("&About"), this, SLOT(about()));
	help_menu->addAction(tr("About &Qt"), qApp, SLOT(aboutQt()));
#else
	//maemo only supports a limited number of menu items
	//TODO: correct for Qt 4.6?
	menuBar()->addAction(tr("&Statistics"), this, SLOT(showStatistics()));
	menuBar()->addAction(tr("&About"), this, SLOT(about())); menuBar()->addAction(new_game);
	menuBar()->addAction(undo); //not visible when disabled, so we'll move it to the end to avoid confusion
#endif

	connect(new_game, SIGNAL(triggered()),
		gamewidget, SLOT(restart()));
	connect(undo, SIGNAL(triggered()),
		gamewidget, SLOT(undo()));
	connect(use_animation, SIGNAL(toggled(bool)),
		gamewidget, SLOT(enableAnimations(bool)));
	connect(use_sound, SIGNAL(toggled(bool)),
		gamewidget, SLOT(enableSound(bool)));

	connect(gamewidget, SIGNAL(enableUndo(bool)),
		undo, SLOT(setEnabled(bool)));
	connect(gamewidget, SIGNAL(showPoints(int)),
		this, SLOT(showPoints(int)));
	connect(gamewidget, SIGNAL(newScore(int)),
		this, SLOT(setScore(int)));

	restoreState(settings.value("window_state", saveState()).toByteArray());
	resize(settings.value("window_size", size()).toSize());
	move(settings.value("window_pos", pos()).toPoint());
}


void MainWindow::about() {
	QMessageBox::about(this, tr("About Blubbels"),
		tr("<center><h1>Blubbels 1.0</h1>\
A Jawbreaker&trade; clone written in Qt 4\
<p>The current version of Blubbels can be found on <a href=\"http://sourceforge.net/projects/blubbels/\">http://sourceforge.net/projects/blubbels/</a>.</p>\
<small><p>&copy;2007-2010 Christian Pulvermacher &lt;pulvermacher@gmx.de&gt;<br>German Translation: &copy;2009 Dominic Hopf &lt;dh@dmaphy.de&gt;</p></small></center>\
%1")
#if !MAEMO
	.arg("<p>This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.</p> <p>This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.</p> <p>You should have received a copy of the GNU General Public License along with this program; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.</p>")
#else
	.arg("<p>This program is free software; License: <a href=\"http://www.gnu.org/licenses/gpl-2.0.html\">GNU GPL 2</a> or later.</p>")
#endif
	);
}


void MainWindow::help()
{
#if !MAEMO
	QMessageBox::about(this, tr("How to Play"),
		tr("<b>How to play Blubbels</b><br>\
<p>In the main window you can see lots of colored bubbles, arranged in 'stacks'.</p>\
<p>If there are two or more bubbles of the same color next to each other (either horizontally or vertically) they can be removed. You can see the points that will be awarded for doing so by hovering your mouse cursor over the bubbles.</p>\
<p>Remove the group of bubbles by clicking on them. Any bubbles that rest above these will then fall down.  After completely emptying a stack, a new stack of bubbles of random height will be moved in from the left.</p>\
<p>The game ends if there are no bubbles left on the gameboard that can be removed.</p>"));
#endif
}


//window closed, save settings
void MainWindow::closeEvent(QCloseEvent* /*ev*/)
{
	//state of use_sound is saved by GameWidget::enableSound()
	settings.setValue("use_animation", use_animation->isChecked());
	settings.setValue("window_state", saveState());
	settings.setValue("window_size", size());
	settings.setValue("window_pos", pos());
	settings.sync();
}


//display current score in status bar (independent of other messages)
void MainWindow::setScore(int score)
{
#if MAEMO
	setWindowTitle(tr("Blubbels - Score: %n", "", score));
#else
	score_label->setText(tr("Score: %n", "", score));
#endif
}


void MainWindow::showPoints(int points)
{
#if !MAEMO
	if(points)
		statusBar()->showMessage(tr("%1 Points").arg(points));
	else
		statusBar()->clearMessage();
#endif
}


void MainWindow::showStatistics()
{
	QMessageBox::information(this, tr("Statistics"),
	tr("<table>\
<tr><td>Average:</td><td align='right'>%1</td></tr>\
<tr><td>Highscore:</td><td align='right'>%2</td></tr>\
<tr><td>Games played:</td><td align='right'>%3</td></tr>\
</table>")
	.arg(int(settings.value("average", 0).toDouble()))
	.arg(settings.value("highscore", 0).toInt())
	.arg(settings.value("games_played", 0).toInt()));
}


int main(int argc, char* argv[])
{
	QApplication app(argc, argv);

	QCoreApplication::setOrganizationName("Blubbels");
	QCoreApplication::setApplicationName("Blubbels");

	QTranslator translator;

#ifndef USE_SHARED_DIR
	//use pwd
	QString path = "";
#else
#if !MAEMO
	QString path = "/usr/share/games/blubbels/";
#else
	QString path = "/usr/share/blubbels/"; 
#endif
#endif
	translator.load(path + "blubbels_" + QLocale::system().name());
	app.installTranslator(&translator);

	MainWindow *mw = new MainWindow;
	mw->show();
	return app.exec();
}
