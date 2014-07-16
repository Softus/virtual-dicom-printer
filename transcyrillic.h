/*
 * Copyright (C) 2013-2014 Irkutsk Diagnostic Center.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TRANSCYRILLIC_H
#define TRANSCYRILLIC_H

#include <QString>

// Convert russian names written in latin symbols back to cyrillic
//
// IVANOV => ИВАНОВ
// NEPOMNYASHCHIKH => НЕПОМНЯЩИХ
//
QString translateToCyrillic(const QString& str);
QString translateToLatin(const QString& str);

#endif // TRANSCYRILLIC_H
