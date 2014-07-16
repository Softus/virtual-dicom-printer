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

#if defined(_MSC_VER)
#pragma execution_character_set("utf-8")
#endif
#include "transcyrillic.h"

static inline int next(const QString& str, int idx)
{
    return str.length() > ++idx? str[idx].toLower().unicode(): 0;
}

QString translateToCyrillic(const QString& str)
{
    if (str.isNull())
    {
        return str;
    }

    QString ret;

    for (int i = 0; i < str.length(); ++i)
    {
        switch (str[i].unicode())
        {
        case '^':
            ret.append(' '); // Заменяем шапку на пробел
            break;
        case 'A':
            ret.append(L'А');
            break;
        case 'B':
            ret.append(L'Б');
            break;
        case 'V':
            ret.append(L'В');
            break;
        case 'G':
            ret.append(L'Г');
            break;
        case 'D':
            ret.append(L'Д');
            break;
        case 'E':
            ret.append(i==0?L'Э':L'Е'); // В начале слова русская 'е' это 'ye'
            break;
        case 'Z':
            ret.append(next(str, i) == 'h'? ++i, L'Ж': L'З');
            break;
        case 'I':
            ret.append(L'И');
            break;
        case 'K':
            ret.append(next(str, i) == 'h'? ++i, L'Х': L'К');
            break;
        case 'L':
            ret.append(L'Л');
            break;
        case 'M':
            ret.append(L'М');
            break;
        case 'N':
            ret.append(L'Н');
            break;
        case 'O':
            ret.append(L'О');
            break;
        case 'P':
            ret.append(L'П');
            break;
        case 'R':
            ret.append(L'Р');
            break;
        case 'S':
            if (next(str, i) == 'h')
            {
                ++i;
                if (next(str, i) == 'c' && next(str, i + 1) == 'h')
                {
                    i+=2;
                    ret.append(L'Щ');
                }
                else
                {
                    ret.append(L'Ш');
                }
            }
            else
            {
                ret.append(L'С');
            }
            break;
        case 'T':
            ret.append(next(str, i) == 's'? ++i, L'Ц': L'Т');
            break;
        case 'U':
            ret.append(L'У');
            break;
        case 'F':
            ret.append(L'Ф');
            break;
        case 'X':
            ret.append(L'К').append(i < str.length() - 1 && str[i+1].isLower()? L'с': L'С');
            break;
        case 'C':
            ret.append(next(str, i) == 'h'? ++i, L'Ч': L'К'); // Английская 'c' без 'h' не должна встречаться.
            break;
        case 'Y':
            switch (next(str, i)) {
            case 'e':
                ++i, ret.append(L'Е');
                break;
            case 'o':
                ++i, ret.append(L'Ё');
                break;
            case 'u':
                ++i, ret.append(L'Ю');
                break;
            case 'i':
                ret.append(L'Ь'); // Только для мягкого знака перед 'и', как Ilyin.
                break;
            case 'a':
                ++i, ret.append(L'Я');
                break;
            case '\x0':
            case ' ':
            case '.':
                ret.append(L'И').append(L'Й'); // Фамилии на ый заканчиваются редко
                break;
            default:
                ret.append(L'Ы');
                break;
            }
            break;

        case 'a':
            ret.append(L'а');
            break;
        case 'b':
            ret.append(L'б');
            break;
        case 'v':
            ret.append(L'в');
            break;
        case 'g':
            ret.append(L'г');
            break;
        case 'd':
            ret.append(L'д');
            break;
        case 'e':
            ret.append(i==0?L'э':L'е'); // В начале слова русская 'е' это 'ye'
            break;
        case 'z':
            ret.append(next(str, i) == 'h'? ++i, L'ж': L'з');
            break;
        case 'i':
            ret.append(L'и');
            break;
        case 'k':
            ret.append(next(str, i) == 'h'? ++i, L'х': L'к');
            break;
        case 'l':
            ret.append(L'л');
            break;
        case 'm':
            ret.append(L'м');
            break;
        case 'n':
            ret.append(L'н');
            break;
        case 'o':
            ret.append(L'о');
            break;
        case 'p':
            ret.append(L'п');
            break;
        case 'r':
            ret.append(L'р');
            break;
        case 's':
            if (next(str, i) == 'h')
            {
                ++i;
                if (next(str, i) == 'c' && next(str, i + 1) == 'h')
                {
                    i+=2;
                    ret.append(L'щ');
                }
                else
                {
                    ret.append(L'ш');
                }
            }
            else
            {
                ret.append(L'с');
            }
            break;
        case 't':
            ret.append(next(str, i) == 's'? ++i, L'ц': L'т');
            break;
        case 'u':
            ret.append(L'у');
            break;
        case 'f':
            ret.append(L'ф');
            break;
        case 'x':
            ret.append(L'к').append(L'с');
            break;
        case 'с':
            ret.append(next(str, i) == 'h'? ++i, L'ч': L'к'); // Английская 'c' без 'h' не должна встречаться.
            break;
        case 'y':
            switch (next(str, i)) {
            case 'e':
                ++i, ret.append(L'е');
                break;
            case 'o':
                ++i, ret.append(L'ё');
                break;
            case 'u':
                ++i, ret.append(L'ю');
                break;
            case 'i':
                ret.append(L'ь'); // Только для мягкого знака перед 'и', как Ilyin.
                break;
            case 'a':
                ++i, ret.append(L'я');
                break;
            case '\x0':
            case ' ':
            case '.':
                ret.append(L'и').append(L'й'); // Фамилии на ый заканчиваются редко
                break;
            default:
                ret.append(L'ы');
                break;
            }
            break;

        default:
            ret.append(str[i]); // Прочие символы без изменений
            break;
        }
    }

    return ret;
}

QString translateToLatin(const QString& str)
{
    if (str.isNull())
    {
        return str;
    }

    QString ret;

    for (int i = 0; i < str.length(); ++i)
    {
        switch (str[i].unicode())
        {
//      case ' ':
//          ret.append('^'); // Заменяем пробел на шапку
//          break;
        case L'А':
            ret.append('A');
            break;
        case L'Б':
            ret.append('B');
            break;
        case L'В':
            ret.append('V');
            break;
        case L'Г':
            ret.append('G');
            break;
        case L'Д':
            ret.append('D');
            break;
        case L'Е':
            if (i==0)  // В начале слова русская 'е' это 'ye' или 'YE'
            {
                ret.append('Y');
                ret.append(str.length() > 1 && str[1].isLower()? 'e': 'E');
            }
            else
            {
                ret.append('E');
            }
            break;
        case L'Ё':
            ret.append('Y');
            ret.append(str.length() > i && str[i].isLower()? 'o': 'O');
            break;
        case L'Ж':
            ret.append('Z');
            ret.append(str.length() > i && str[i].isLower()? 'h': 'H');
            break;
        case L'З':
            ret.append('Z');
            break;
        case L'И': // В конце слова 'ий' == 'y'
            if (str.length() > i && str[i] == L'Й')
            {
                ret.append('Y'); ++i;
            }
            else if (str.length() > i && str[i] == L'й')
            {
                ret.append('y'); ++i;
            }
            else
            {
                ret.append('I');
            }
            break;
        case L'Й':
            ret.append('Y');
            break;
        case L'К':
            ret.append('K');
            break;
        case L'Л':
            ret.append('L');
            break;
        case L'М':
            ret.append('M');
            break;
        case L'Н':
            ret.append('N');
            break;
        case L'О':
            ret.append('O');
            break;
        case L'П':
            ret.append('P');
            break;
        case L'Р':
            ret.append('R');
            break;
        case L'С':
            ret.append('S');
            break;
        case L'Т':
            ret.append('T');
            break;
        case L'У':
            ret.append('U');
            break;
        case L'Ф':
            ret.append('F');
            break;
        case L'Х':
            ret.append('K');
            ret.append(str.length() > i && str[i].isLower()? 'h': 'H');
            break;
        case L'Ц':
            ret.append('T');
            ret.append(str.length() > i && str[i].isLower()? 's': 'S');
            break;
        case L'Ч':
            ret.append('C');
            ret.append(str.length() > i && str[i].isLower()? 'h': 'H');
            break;
        case L'Ш':
            ret.append('S');
            ret.append(str.length() > i && str[i].isLower()? 'h': 'H');
            break;
        case L'Щ':
            ret.append('S');
            ret.append(str.length() > i && str[i].isLower()? "hch": "HCH");
            break;
        case L'Ъ': // Съедаем
            break;
        case L'Ы': // В конце слова 'ый' == 'y'
            ret.append('Y');
            if (str.length() > i && str[i] == L'Й')
            {
                ++i;
            }
            break;
        case L'Ь': // Только для мягкого знака перед 'и', как Ilyin.
            if (str.length() > i && str[i] == L'И')
            {
                ret.append('Y');
            }
            else if (str.length() > i && str[i] == L'и')
            {
                ret.append('y');
            }
            break;
        case L'Э':
            ret.append('E');
            break;
        case L'Ю':
            ret.append('Y');
            ret.append(str.length() > i && str[i].isLower()? 'u': 'U');
            break;
        case L'Я':
            ret.append('Y');
            ret.append(str.length() > i && str[i].isLower()? 'a': 'A');
            break;

        case L'а':
            ret.append('a');
            break;
        case L'б':
            ret.append('b');
            break;
        case L'в':
            ret.append('v');
            break;
        case L'г':
            ret.append('g');
            break;
        case L'д':
            ret.append('d');
            break;
        case L'е':
            if (i==0)  // В начале слова русская 'е' это 'ye' или 'YE'
            {
                ret.append('y');
            }
            ret.append('e');
            break;
        case L'ё':
            ret.append("yo");
            break;
        case L'ж':
            ret.append("zh");
            break;
        case L'з':
            ret.append('z');
            break;
        case L'и': // В конце слова 'ий' == 'y'
            if (str.length() > i && str[i] == L'й')
            {
                ret.append('y'); ++i;
            }
            else
            {
                ret.append('i');
            }
            break;
        case L'й':
            ret.append('y');
            break;
        case L'к':
            ret.append('k');
            break;
        case L'л':
            ret.append('l');
            break;
        case L'м':
            ret.append('m');
            break;
        case L'н':
            ret.append('n');
            break;
        case L'о':
            ret.append('o');
            break;
        case L'п':
            ret.append('p');
            break;
        case L'р':
            ret.append('r');
            break;
        case L'с':
            ret.append('s');
            break;
        case L'т':
            ret.append('t');
            break;
        case L'у':
            ret.append('u');
            break;
        case L'ф':
            ret.append('f');
            break;
        case L'х':
            ret.append("kh");
            break;
        case L'ц':
            ret.append("tc");
            break;
        case L'ч':
            ret.append("ch");
            break;
        case L'ш':
            ret.append("sh");
            break;
        case L'щ':
            ret.append("shch");
            break;
        case L'ъ': // Съедаем
            break;
        case L'ы': // В конце слова 'ый' == 'y'
            ret.append('y');
            if (str.length() > i && str[i] == L'й')
            {
                ++i;
            }
            break;
        case L'ь': // Только для мягкого знака перед 'и', как Ilyin.
            if (str.length() > i && str[i] == L'и')
            {
                ret.append('y');
            }
            break;
        case L'э':
            ret.append('e');
            break;
        case L'ю':
            ret.append("yu");
            break;
        case L'я':
            ret.append("ya");
            break;

        default:
            ret.append(str[i]); // Прочие символы без изменений
            break;
        }
    }

    return ret;
}
