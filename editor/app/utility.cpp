// Copyright (c) 2010-2020 Sami Väisänen, Ensisoft
//
// http://www.ensisoft.com
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include "config.h"

#include "warnpush.h"
#  include <QDir>
#  include <QByteArray>
#  include <QtGlobal>
#include "warnpop.h"

#if defined(LINUX_OS)
#  include <langinfo.h>
#endif

#include <cstring>

#include "utility.h"

namespace app
{

QString JoinPath(const QString& lhs, const QString& rhs)
{
    const auto p = lhs + "/" + rhs;
    return QDir::toNativeSeparators(QDir::cleanPath(p));
}

QString CleanPath(const QString& path)
{
    return QDir::toNativeSeparators(QDir::cleanPath(path));
}

bool MakePath(const QString& path)
{
    QDir dir(path);
    return dir.mkpath(path);
}

QString FromUtf8(const std::string& str)
{
    return QString::fromUtf8(str.c_str());
}

QString FromLatin(const std::string& str)
{
    return QString::fromLatin1(str.c_str());
}

QString FromLocal8Bit(const std::string& s)
{
#if defined(WINDOWS_OS)
    return QString::fromUtf8(s.c_str());
#elif defined(LINUX_OS)
    const char* codeset = nl_langinfo(CODESET);
    if (!std::strcmp(codeset, "UTF-8"))
        return FromUtf8(s);

    return QString::fromLocal8Bit(s.c_str());
#endif
}

std::string ToUtf8(const QString& str)
{
    const auto& bytes = str.toUtf8();
    return std::string(bytes.data(), bytes.size());
}

std::string ToLatin(const QString& str)
{
    const auto& bytes = str.toLatin1();
    return std::string(bytes.data(), bytes.size());
}

std::string ToLocal8Bit(const QString& str)
{
#if defined(WINDOWS_OS)
    return ToUtf8(str);
#elif defined(LINUX_OS)
    const char* codeset = nl_langinfo(CODESET);
    if (!std::strcmp(codeset, "UTF-8"))
        return ToUtf8(str);

    const auto& bytes = str.toLocal8Bit();
    return std::string(bytes.data(),
        bytes.size());

#else
#  error Unimplemented method
#endif
}

std::ofstream OpenBinaryOStream(const QString& file)
{
#if defined(WINDOWS_OS)
    std::ofstream out(file.toStdWString(), std::ios::out | std::ios::binary | std::ios::trunc);
#elif defined(LINUX_OS)
    std::ofstream out(ToLocal8Bit(file), std::ios::out | std::ios::binary | std::ios::trunc);
#else
#  error Unimplemented function
#endif
    return out;
}

std::ifstream OpenBinaryIStream(const QString& file)
{
#if defined(WINDOWS_OS)
    std::ifstream in(file.toStdWString(), std::ios::in | std::ios::binary);
#elif defined(LINUX_OS)
    std::ifstream in(ToLocal8Bit(file), std::ios::in | std::ios::binary);
#else
    error unimplemented function
#endif
    return in;

}

bool WriteAsBinary(const QString& file, const void* data, std::size_t bytes)
{
    std::ofstream out = OpenBinaryOStream(file);
    if (!out.is_open())
        return false;

    out.write(static_cast<const char*>(data), bytes);
    return true;
}

QString RandomString()
{
   const QString possibleCharacters("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789");
   const int randomStringLength = 12; // assuming you want random strings of 12 characters

   QString randomString;
   for(int i=0; i<randomStringLength; ++i)
   {
       int index = qrand() % possibleCharacters.length();
       QChar nextChar = possibleCharacters.at(index);
       randomString.append(nextChar);
   }
   return randomString;
}

static QString gAppHome;

void InitializeAppHome(const QString& appname)
{
    const auto& home = QDir::homePath();
    const auto& appdir = home + "/" + appname;

    // if we cannot create the directory we're fucked so then throw.
    QDir dir;
    if (!dir.mkpath(appdir))
        throw std::runtime_error(ToUtf8(QString("failed to create %1").arg(appdir)));

    gAppHome = QDir::cleanPath(appdir);
}

QString GetAppFilePath(const QString& name)
{
    // pathstr is an absolute path so then this is also
    // an absolute path.
    return QDir::toNativeSeparators(gAppHome + "/" + name);
}

} // namespace