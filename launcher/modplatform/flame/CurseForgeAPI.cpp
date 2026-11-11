// SPDX-License-Identifier: GPL-3.0-only
/*
 *  Galaxy Launcher - Minecraft Launcher
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "CurseForgeAPI.h"

#include <QRegularExpression>
#include <QUrlQuery>

#include <algorithm>

#include "BuildConfig.h"
#include "Json.h"
#include "modplatform/flame/FlamePackIndex.h"

namespace {

/** Lower case letters and digits only: "Just Enough Items (JEI)" and "just-enough-items" compare alike. */
QString squash(const QString& text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar c : text) {
        if (c.isLetterOrNumber()) {
            out += c.toLower();
        }
    }
    return out;
}

QStringList words(const QString& text)
{
    static const QRegularExpression separators(QStringLiteral("[^\\p{L}\\p{N}]+"));
    QStringList out;
    for (const auto& word : text.toLower().split(separators, Qt::SkipEmptyParts)) {
        out << word;
    }
    return out;
}

}  // namespace

QList<ResourceAPI::SortingMethod> CurseForgeAPI::getSortingMethods() const
{
    // https://docs.curseforge.com/rest-api/#tocS_ModsSearchSortField (Relevance is ours, see the class comment)
    return { { .index = RelevanceSort, .name = "Relevance", .readableName = QObject::tr("Sort by Relevance") },
             { .index = 2, .name = "Popularity", .readableName = QObject::tr("Sort by Popularity") },
             { .index = 6, .name = "TotalDownloads", .readableName = QObject::tr("Sort by Downloads") },
             { .index = 3, .name = "LastUpdated", .readableName = QObject::tr("Sort by Last Updated") },
             { .index = 11, .name = "ReleasedDate", .readableName = QObject::tr("Sort by Newest") },
             { .index = 4, .name = "Name", .readableName = QObject::tr("Sort by Name") } };
}

QUrl CurseForgeAPI::searchUrl(const SearchArgs& args)
{
    QUrlQuery query;
    query.addQueryItem("gameId", "432");
    query.addQueryItem("classId", QString::number(classIdFor(args.type)));
    query.addQueryItem("index", QString::number(args.offset));
    query.addQueryItem("pageSize", QString::number(PageSize));
    const QString text = args.search.value_or(QString()).trimmed();
    if (!text.isEmpty()) {
        query.addQueryItem("searchFilter", text);
    }
    const unsigned int sort = args.sorting.has_value() ? args.sorting->index : RelevanceSort;
    // Relevance: the most popular matches, ordered by name match afterwards.
    query.addQueryItem("sortField", QString::number(sort == RelevanceSort ? 2 : sort));
    query.addQueryItem("sortOrder", sort == 4 ? "asc" : "desc");
    if (args.loaders.has_value()) {
        auto loaders = args.loaders.value();
        loaders &= ~static_cast<std::uint16_t>(ModPlatform::ModLoaderType::DataPack);
        if (loaders != 0) {
            query.addQueryItem("modLoaderTypes", modLoaderFilter(loaders));
        }
    }
    if (args.categoryIds.has_value() && !args.categoryIds->empty()) {
        query.addQueryItem("categoryIds", QStringLiteral("[%1]").arg(args.categoryIds->join(',')));
    }
    if (args.versions.has_value() && !args.versions->empty()) {
        query.addQueryItem("gameVersion", args.versions->front().toString());
    }
    QUrl url(BuildConfig.FLAME_BASE_URL + "/mods/search");
    // Encode everything that is not plain: spaces, '+', '&', '[' and ']' must reach CurseForge as typed.
    QString encoded;
    for (const auto& [key, value] : query.queryItems(QUrl::FullyDecoded)) {
        if (!encoded.isEmpty()) {
            encoded += '&';
        }
        encoded += QString::fromUtf8(QUrl::toPercentEncoding(key)) + '=' + QString::fromUtf8(QUrl::toPercentEncoding(value));
    }
    url.setQuery(encoded, QUrl::StrictMode);
    return url;
}

int CurseForgeAPI::relevance(const QString& search, const QString& name, const QString& slug, const QString& summary)
{
    const QString wanted = squash(search);
    if (wanted.isEmpty()) {
        return 0;
    }
    const QString title = squash(name);
    if (title == wanted || squash(slug) == wanted) {
        return 1000;
    }
    // "JEI" finds "Just Enough Items (JEI)": a word of the name, or its initials.
    const auto nameWords = words(name);
    QString initials;
    for (const auto& word : nameWords) {
        initials += word.front();
    }
    if (nameWords.contains(search.trimmed().toLower()) || initials == wanted) {
        return 800;
    }
    if (title.startsWith(wanted) || squash(slug).startsWith(wanted)) {
        return 600;
    }
    const auto wantedWords = words(search);
    int inName = 0;
    for (const auto& word : wantedWords) {
        if (title.contains(word)) {
            inName++;
        }
    }
    if (!wantedWords.isEmpty() && inName == wantedWords.size()) {
        return 400;
    }
    if (inName > 0) {
        return 200 * inName / static_cast<int>(wantedWords.size());
    }
    return squash(summary).contains(wanted) ? 50 : 0;
}

Net::RPC::Spec<QList<ModPlatform::IndexedPack>> CurseForgeAPI::searchProjects(const SearchArgs& args) const
{
    const int classId = classIdFor(args.type);
    const QString text = args.search.value_or(QString()).trimmed();
    const bool rank = !text.isEmpty() && (!args.sorting.has_value() || args.sorting->index == RelevanceSort);
    return { { .url = searchUrl(args) }, [classId, text, rank](const auto& response) -> Result<QList<ModPlatform::IndexedPack>> {
                TRY_INTO(auto data, Json::requireDocument(response, "CurseForge project search")
                                        .and_then([](const auto& v) { return Json::requireObject(v); })
                                        .and_then([](const auto& v) { return Json::requireArray(v, "data"); }))

                struct Ranked {
                    ModPlatform::IndexedPack pack;
                    int score;
                };
                QList<Ranked> found;
                for (const auto& entry : data) {
                    const auto object = entry.toObject();
                    // Only the requested kind of project: no modpacks (or worlds, or plugins) among mods.
                    if (classId != 0 && object.value("classId").toInt(classId) != classId) {
                        continue;
                    }
                    ModPlatform::IndexedPack pack;
                    TRY(Flame::Parse::loadIndexedPack(pack, object))
                    const int score = rank ? relevance(text, pack.name, pack.slug, pack.description) : 0;
                    found.append({ std::move(pack), score });
                }
                // Stable: equally relevant projects keep CurseForge's popularity order.
                std::stable_sort(found.begin(), found.end(), [](const Ranked& a, const Ranked& b) { return a.score > b.score; });
                QList<ModPlatform::IndexedPack> packs;
                packs.reserve(found.size());
                for (auto& ranked : found) {
                    packs.append(std::move(ranked.pack));
                }
                return packs;
            } };
}
