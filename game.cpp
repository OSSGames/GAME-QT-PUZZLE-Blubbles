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

#include <QColor>
#include <QFileInfo>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QSettings>
#include <QSound>
#include <QTimer>

#include <cstdlib>


#define MAEMO (defined Q_WS_HILDON || defined Q_WS_MAEMO_5)


const QColor bubbleColor[] = {
	Qt::white, //"empty", shouldn't be shown
	Qt::green,
	Qt::blue,
	Qt::red,
	QColor(255, 180, 0),
	Qt::gray
};

const float initial_speed = 0.07;


GameWidget::GameWidget(QWidget* parent) :
	QWidget(parent),
	bubblesize(0),
	pop(0),
	timer(new QTimer(this))
{
	setMouseTracking(true);

	setAttribute(Qt::WA_OpaquePaintEvent);
	setMinimumSize(90, 90);

	connect(timer, SIGNAL(timeout()),
		this, SLOT(animationStep()));
	timer->setInterval(50);

	srand(time(0));
	restart();
}


//translates cursor position into grid coordinates
Box GameWidget::getGridPos(int x, int y)
{
	Box result; //invalid default value
	//make coords relative to bubbles
	x -= xoff; y -= yoff;

	if(x >=0 and y >= 0
	and (x-margin/2) / (bubblesize+margin) < grid_size and (y-margin/2) / (bubblesize+margin) < grid_size
	and grid[(x-margin/2)/(bubblesize+margin)][(y-margin/2)/(bubblesize+margin)].color) {
		result.x = (x-margin/2) / (bubblesize+margin);
		result.y = (y-margin/2) / (bubblesize+margin);
	}
	return result;
}


//advances all animations by one frame
void GameWidget::animationStep()
{
	bool animations_found = false;
	for(int x = 0; x < grid_size; x++) {
		for(int y = 0; y < grid_size; y++) {
			if(grid[x][y].yoffset) {
				grid[x][y].yoffset -= grid[x][y].yspeed;
				grid[x][y].yspeed *= 1.7;

				if((grid[x][y].yspeed > 0 and grid[x][y].yoffset <= 0)
				or (grid[x][y].yspeed < 0 and grid[x][y].yoffset >= 0))
					grid[x][y].yoffset = 0;
				else
					animations_found = true;
			}
			if(grid[x][y].xoffset) {
				grid[x][y].xoffset -= grid[x][y].xspeed;
				grid[x][y].xspeed *= 1.7;

				if((grid[x][y].xspeed > 0 and grid[x][y].xoffset <= 0)
				or (grid[x][y].xspeed < 0 and grid[x][y].xoffset >= 0))
					grid[x][y].xoffset = 0;
				else
					animations_found = true;
			}
		}
	}

	if(!animations_found)
		timer->stop();
	update();
}


//let bubbles fall down / move stacks to right
void GameWidget::compressBubbles()
{
	for(int x = grid_size-1; x >= 0; x--) {
		int hole = -1;
		int holes = 0;
		bool bubble_found = false;
		for(int y = 0; y <= grid_size; y++) {
			if(y == grid_size or grid[x][y].color) { //not empty
				if(holes) { //hole found, move bubbles down
					for(int i = hole; i > 0; i--) {
						grid[x][i] = grid[x][i-holes];

						if(use_animations) {
							grid[x][i].yoffset = holes;
							grid[x][i].yspeed = initial_speed;
						}
					}
					for(int i = 0; i < holes; i++)
						grid[x][i] = Bubble();
				}
				bubble_found = true;
				hole = -1;
				holes = 0;
			} else if(bubble_found) {
				hole = y;
				holes++;
			}
		}
	}

	for(int x = grid_size-1; x >= 0; x--) {
		if(!grid[x][grid_size-1].color) { //collumn empty
			//shift remaining collums
			int x2 = x;
			while(--x2 >= 0) {
				for(int y = 0; y < grid_size; y++) {
					grid[x2+1][y] = grid[x2][y];

					if(use_animations) {
						grid[x2+1][y].xoffset += 1;
						grid[x2+1][y].xspeed = initial_speed;
					}
				}
			}

			//add new col
			const int height = int(double(rand())/(RAND_MAX + 1.0)*grid_size);
			for(int y = grid_size-1; y >= height; y--) {
				grid[0][y] = Bubble(double(rand())/(RAND_MAX + 1.0)*num_colors+1);

				if(use_animations) {
					//place new col off screen
					grid[0][y].xoffset = (width() - grid_size*(bubblesize+margin))/bubblesize/2;
					grid[0][y].xspeed = initial_speed;
				}
			}

			//erase remains of old collumn
			for(int y = height-1; y >= 0; y--)
				grid[0][y] = Bubble();

			x++; //make sure to run over collumn again
		}
	}
	if(use_animations)
		timer->start();

	update();
	checkGameOver();
}


//returns list of all bubbles connected to pos
void GameWidget::getConnectedBubbles(Box pos, QList<Box> &list)
{
	int x = pos.x; int y = pos.y;
	const short color = grid[x][y].color;
	if(!color) //nothing to select, exit
		return;

	list << pos;

	x--; //left
	if(x >= 0 and color == grid[x][y].color and !list.contains(Box(x, y))) {
		getConnectedBubbles(Box(x, y), list);
	}
	x += 2; //right
	if(x < grid_size and color == grid[x][y].color and !list.contains(Box(x, y))) {
		getConnectedBubbles(Box(x, y), list);
	}
	x--; y--; //up
	if(y >= 0 and color == grid[x][y].color and !list.contains(Box(x, y))) {
		getConnectedBubbles(Box(x, y), list);
	}
	y += 2; //down
	if(y < grid_size and color == grid[x][y].color and !list.contains(Box(x, y))) {
		getConnectedBubbles(Box(x, y), list);
	}
}


void GameWidget::checkGameOver()
{
	for(int x = 0; x < grid_size; x++) {
		for(int y = 0; y < grid_size; y++) {
			QList<Box> connected;
			getConnectedBubbles(Box(x,y), connected);
			if(connected.size() > 1)
				return; //found cluster, continue game
		}
	}
	//assertion: no connected bubbles left in grid

	//update stats
	QSettings settings;
	settings.setValue("average",
		(settings.value("average", 0).toDouble()*settings.value("games_played", 0).toInt() + score) / (settings.value("games_played", 0).toInt()+1));
	settings.setValue("games_played", settings.value("games_played", 0).toInt()+1);

	if(score > settings.value("highscore", 0).toInt()) {
		QMessageBox::information(this, tr("New Highscore"), tr("Your Score: %n<br>Congratulations!", "", score) + tr("<br>You broke the previous record of %n points!", "", settings.value("highscore", 0).toInt()));
		settings.setValue("highscore", score);
	} else {
		QMessageBox::information(this, tr("Game Over"), tr("Your Score: %n<br>Congratulations!", "", score));
	}

	restart();
}


void GameWidget::mouseMoveEvent(QMouseEvent *event)
{
	if(event->buttons())
		return; //don't change selection while mouse button is down

	const Box hovered = getGridPos(event->x(), event->y());
	if(hovered.x == -1) { //nothing under cursor
		emit showPoints();
		selection.clear();
	} else if(!selection.contains(hovered)) { //cursor not in old selection
		selection.clear();
		getConnectedBubbles(hovered, selection);
		const int n = selection.size();
		if(n > 1) {
			emit showPoints(n*n);
		} else {
			emit showPoints();
			selection.clear(); //don't select single bubbles
		}
	}
	update();
}


void GameWidget::mouseReleaseEvent(QMouseEvent *event)
{
	if(selection.size() < 2 or !selection.contains(getGridPos(event->x(), event->y())))
		return; //nothing selected or mouse pointer not in selection

#if !MAEMO
	if(use_sound)
		pop->play(); //pop is initialized by enableSound()
#endif

	//backup
	oldscore = score;
	for(int x = 0; x < grid_size; x++)
		for(int y = 0; y < grid_size; y++)
			old_grid[x][y] = grid[x][y];

	//remove bubbles
	foreach(Box newpos, selection) {
		grid[newpos.x][newpos.y] = Bubble();
	}

	score += selection.size()*selection.size();
	selection.clear();

	emit enableUndo(true);
	emit newScore(score);
	emit showPoints();

	compressBubbles();

	mouseMoveEvent(event); //draw selection if there's a new set of connected bubbles
}


void GameWidget::resizeEvent(QResizeEvent* /*event*/)
{
	const int old_bubblesize = bubblesize;
	const int size = qMin(width(), height());
	bubblesize = 3.0*size/(4.0*grid_size+1); //best size for 1/3 margin
	margin = bubblesize/5; //separates bubbles from each other and the widget border

	const int round_error = size % int(grid_size*bubblesize + (grid_size+1)*margin);
	xoff = (width()-size+round_error)/2;
	yoff = (height()-size+round_error)/2;

	if(bubblesize != old_bubblesize) {
		//generate new pixmaps
		for(int i = 1; i <= num_colors; i++) {
			bubblePixmap[i] = QPixmap(bubblesize, bubblesize);
			bubblePixmap[i].fill(Qt::transparent);

			QPainter painter(&bubblePixmap[i]);
			painter.setPen(Qt::black);
			painter.setRenderHint(QPainter::Antialiasing, true);

			QRadialGradient radialGrad(QPointF(0.3*bubblesize, 0.3*bubblesize), 0.8*bubblesize);
			radialGrad.setColorAt(0, Qt::white);
			radialGrad.setColorAt(0.4, bubbleColor[i]);
			radialGrad.setColorAt(1, Qt::black);

			painter.setBrush(QBrush(radialGrad));
			painter.drawEllipse(0, 0, bubblesize, bubblesize);
		}
	}

	emit showPoints();
	update();
}


//enable sound and initialize sound system
void GameWidget::enableSound(bool on)
{
#if !MAEMO
	use_sound = on;

	QString path = "";
#ifdef USE_SHARED_DIR
	path = "/usr/share/games/blubbels/";
#endif
	const static QString soundfile = path + "blubb.wav";
	if(use_sound and !pop) {
		if(!QSound::isAvailable()) {
			use_sound = false;
			//TODO: use MessageBox
			qWarning(qPrintable(tr("Sound system not available, disabling sound!")));
		} else if(QFileInfo(soundfile).isReadable()) {
			pop = new QSound(soundfile);
		} else {
			use_sound = false; //disable sound for now
			qWarning(qPrintable(tr("Couldn't open %1, disabling sound!").arg(soundfile)));
		}
	}

	QSettings settings;
	settings.setValue("use_sound", use_sound);
#endif
}


//start new game, fill grid with bubbles
void GameWidget::restart()
{
	score = 0;
	emit newScore(score);
	emit showPoints();

	emit enableUndo(false);

	selection.clear();

	//seed field with random non-empty bubbles (values 1..num_colors)
	for(int x = 0; x < grid_size; x++) {
		for(int y = 0; y < grid_size; y++) {
			grid[x][y] = Bubble(double(rand())/(RAND_MAX+1.0)*num_colors+1);

			if(use_animations) {
				grid[x][y].xoffset = grid_size + (width() - grid_size*(bubblesize+margin))/bubblesize/2;
				grid[x][y].xspeed = initial_speed;
			}
		}
	}

	if(use_animations)
		timer->start();

	update();
}


//undo the last move
void GameWidget::undo()
{
	emit enableUndo(false);

	score = oldscore;
	emit newScore(score);
	selection.clear();
	emit showPoints();

	if(use_animations) {
		int shift = 0; //number of new collums in the action we want to undo
		for(int x = grid_size-1; x >= shift; x--) {
			bool not_found = false;
			for(int y = 1; y < grid_size; y++) {
				if(grid[x][y].id == old_grid[x-shift][y].id)
					break; //nothing changed

				not_found = true;
				for(int y_old = y-1; y_old >= 0; y_old--) { //search upward
					if(grid[x][y].id == old_grid[x-shift][y_old].id) {
						old_grid[x-shift][y_old].yoffset = y_old - y;
						old_grid[x-shift][y_old].yspeed = -initial_speed;

						not_found = false;
						break; //ids are unique
					}
				}

			}

			if(not_found) {//we couldn't find at least one bubble in it's collum
				shift++; //so it must have been the whole collumn that moved

			}

			if(shift > 0) {
				for(int y = 0; y < grid_size; y++) {
					old_grid[x-shift][y].xoffset = -shift;
					old_grid[x-shift][y].xspeed = -initial_speed;
				}
			}

			//don't skip collumns
			//TODO: only fixes special case shift=2
			if(not_found and shift > 1) {
				for(int y = 0; y < grid_size; y++) {
					old_grid[x-shift+1][y].xoffset = -shift;
					old_grid[x-shift+1][y].xspeed = -initial_speed;
				}
			}
		}
		
		timer->start();
	}

	for(int x = 0; x < grid_size; x++)
		for(int y = 0; y < grid_size; y++)
			grid[x][y] = old_grid[x][y];

	update();
}


void GameWidget::paintEvent(QPaintEvent* /*event*/)
{
	QPainter painter(this);
	painter.fillRect(0, 0, width(), height(), QBrush(Qt::black));
	painter.translate(xoff, yoff);

	for(int i = 0; i < grid_size; i++) {
		for(int j = 0; j < grid_size; j++) {
			if(!grid[i][j].color) //empty
				continue;

			QRectF target(
				(i-grid[i][j].xoffset) * (bubblesize+margin) + margin,
				(j-grid[i][j].yoffset) * (bubblesize+margin) + margin,
				bubblesize, bubblesize);
			QRectF source(0, 0, bubblesize, bubblesize);
			painter.drawPixmap(target, bubblePixmap[grid[i][j].color], source);
		}
	}

	//draw selection
	if(selection.empty() or selection.size() == 1)
		return;

	const short color = grid[selection.first().x][selection.first().y].color;
	painter.setPen(QPen(bubbleColor[color], margin/3, Qt::DotLine, Qt::RoundCap));
	QPainterPath path;

	foreach(Box b, selection) {
		if(b.x == 0 || grid[b.x-1][b.y].color != color) {
			QPointF point(b.x*(bubblesize+margin)+qreal(margin)/2, qreal(b.y*(bubblesize+margin)+margin/2));
			path.moveTo(point);
			point.ry() += bubblesize+margin;
			path.lineTo(point);

		}
		if(b.x == grid_size-1 || grid[b.x+1][b.y].color != color) {
			QPointF point(b.x*(bubblesize+margin)+qreal(margin)*3/2+bubblesize, b.y*(bubblesize+margin)+qreal(margin)/2);
			path.moveTo(point);
			point.ry() += bubblesize+margin;
			path.lineTo(point);
		}
		if(b.y == 0 || grid[b.x][b.y-1].color != color) {
			QPointF point(qreal(b.x*(bubblesize+margin)+margin/2), qreal(b.y*(bubblesize+margin)+margin/2));
			path.moveTo(point);
			point.rx() += bubblesize+margin;
			path.lineTo(point);
		}
		if(b.y == grid_size-1 || grid[b.x][b.y+1].color != color) {
			QPointF point(qreal(b.x*(bubblesize+margin)+margin/2), qreal(b.y*(bubblesize+margin)+margin*3/2+bubblesize));
			path.moveTo(point);
			point.rx() += bubblesize+margin;
			path.lineTo(point);
		}
	}
	painter.drawPath(path);
}
