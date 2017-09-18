/*
** Taiga
** Copyright (C) 2010-2017, Eren Okka
** 
** This program is free software: you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation, either version 3 of the License, or
** (at your option) any later version.
** 
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
** 
** You should have received a copy of the GNU General Public License
** along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <regex>
#include <set>

#include "base/process.h"
#include "base/string.h"
#include "base/url.h"
#include "library/anime_episode.h"
#include "taiga/settings.h"
#include "track/media.h"

namespace track {
namespace recognition {

enum class Stream {
  Unknown,
  Animelab,
  Ann,
  Crunchyroll,
  Daisuki,
  Hidive,
  Plex,
  Veoh,
  Viz,
  Vrv,
  Wakanim,
  Youtube,
};

struct StreamData {
  Stream id;
  enum_t option_id;
  std::wstring name;
  std::regex url_pattern;
  std::regex title_pattern;
};

static const std::vector<StreamData> stream_data{
  // AnimeLab
  {
    Stream::Animelab,
    taiga::kStream_Animelab,
    L"AnimeLab",
    std::regex("animelab\\.com/player/"),
    std::regex("AnimeLab - (.+)"),
  },
  // Anime News Network
  {
    Stream::Ann,
    taiga::kStream_Ann,
    L"Anime News Network",
    std::regex("animenewsnetwork\\.(?:com|cc)/video/[0-9]+"),
    std::regex("(.+) - Anime News Network"),
  },
  // Crunchyroll
  {
    Stream::Crunchyroll,
    taiga::kStream_Crunchyroll,
    L"Crunchyroll",
    std::regex(
      "crunchyroll\\.[a-z.]+/[^/]+/(?:"
        "episode-[0-9]+.*|"
        ".*-(?:movie|ona|ova)"
      ")-[0-9]+"
    ),
    std::regex("Crunchyroll - Watch (?:(.+) - (?:Movie - Movie|ONA - ONA|OVA - OVA)|(.+))"),
  },
  // DAISUKI
  {
    Stream::Daisuki,
    taiga::kStream_Daisuki,
    L"DAISUKI",
    std::regex("daisuki\\.net/[a-z]+/[a-z]+/anime/watch"),
    std::regex("(.+) - DAISUKI"),
  },
  // HIDIVE
  {
    Stream::Hidive,
    taiga::kStream_Hidive,
    L"HIDIVE",
    std::regex("hidive\\.com/stream/"),
    std::regex("(.+)"),
  },
  // Plex
  {
    Stream::Plex,
    taiga::kStream_Plex,
    L"Plex",
    std::regex(
      "plex\\.tv/web/|"
      "localhost:32400/web/|"
      "\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}\\.\\d{1,3}:32400/web/|"
      "plex\\.[a-z0-9-]+\\.[a-z0-9-]+|"
      "[a-z0-9-]+\\.[a-z0-9-]+/plex"
    ),
    std::regex(u8"Plex|(?:\u25B6 )?(.+)"),
  },
  // Veoh
  {
    Stream::Veoh,
    taiga::kStream_Veoh,
    L"Veoh",
    std::regex("veoh\\.com/watch/"),
    std::regex("Watch Videos Online \\| (.+) \\| Veoh\\.com"),
  },
  // Viz Anime
  {
    Stream::Viz,
    taiga::kStream_Viz,
    L"Viz Anime",
    std::regex("viz\\.com/watch/streaming/[^/]+-(?:episode-[0-9]+|movie)/"),
    std::regex("(.+) // VIZ"),
  },
  // VRV
  {
    Stream::Vrv,
    taiga::kStream_Vrv,
    L"VRV",
    std::regex("vrv\\.co/watch"),
    std::regex("VRV - Watch (.+)"),
  },
  // Wakanim
  {
    Stream::Wakanim,
    taiga::kStream_Wakanim,
    L"Wakanim",
    std::regex("wakanim\\.tv/video(?:-premium)?/[^/]+/"),
    std::regex("(.+) / Streaming - Wakanim.TV"),
  },
  // YouTube
  {
    Stream::Youtube,
    taiga::kStream_Youtube,
    L"YouTube",
    std::regex("youtube\\.com/watch"),
    std::regex(u8"YouTube|(?:\u25B6 )?(.+) - YouTube"),
  },
};

const StreamData* FindStreamFromUrl(std::wstring url) {
  EraseLeft(url, L"http://");
  EraseLeft(url, L"https://");

  if (url.empty())
    return nullptr;

  const std::string str = WstrToStr(url);

  for (const auto& item : stream_data) {
    if (std::regex_search(str, item.url_pattern)) {
      const bool enabled = Settings.GetBool(item.option_id);
      return enabled ? &item : nullptr;
    }
  }

  return nullptr;
}

bool ApplyStreamTitleFormat(const StreamData& stream_data, std::string& title) {
  std::smatch match;
  std::regex_match(title, match, stream_data.title_pattern);

  // Use the first non-empty match result
  for (size_t i = 1; i < match.size(); ++i) {
    if (!match.str(i).empty()) {
      title = match.str(i);
      return true;
    }
  }

  // Results are empty, but the match was successful
  if (!match.empty()) {
    title.clear();
    return true;
  }

  return false;
}

void CleanStreamTitle(const StreamData& stream_data, std::string& title) {
  if (!ApplyStreamTitleFormat(stream_data, title))
    return;

  switch (stream_data.id) {
    case Stream::Ann: {
      static const std::regex pattern{" \\((?:s|d)(?:, uncut)?\\)"};
      title = std::regex_replace(title, pattern, "");
      break;
    }
    case Stream::Daisuki: {
      static const std::regex pattern{"(#\\d+ .+) - (.+)"};
      title = std::regex_replace(title, pattern, "$2 - $1");
      break;
    }
    case Stream::Plex: {
      auto str = StrToWstr(title);
      ReplaceString(str, L" \u00B7 ", L"");
      title = WstrToStr(str);
      break;
    }
    case Stream::Vrv: {
      auto str = StrToWstr(title);
      ReplaceString(str, 0, L": EP ", L" - EP ", false, false);
      title = WstrToStr(str);
      break;
    }
    case Stream::Wakanim: {
      auto str = StrToWstr(title);
      ReplaceString(str, 0, L" de ", L" ", false, false);
      ReplaceString(str, 0, L" en VOSTFR", L" VOSTFR", false, false);
      title = WstrToStr(str);
      break;
    }
  }
}

bool GetTitleFromStreamingMediaProvider(const std::wstring& url,
                                        std::wstring& title) {
  const auto stream = FindStreamFromUrl(url);

  if (stream) {
    std::string str = WstrToStr(title);
    CleanStreamTitle(*stream, str);
    title = StrToWstr(str);
  } else {
    title.clear();
  }

  return !title.empty();
}

////////////////////////////////////////////////////////////////////////////////

void IgnoreCommonWebBrowserTitles(const std::wstring& address,
                                  std::wstring& title) {
  const Url url(address);
  if (!url.host.empty() && StartsWith(title, url.host))  // Chrome
    title.clear();
  if (StartsWith(title, L"http://") || StartsWith(title, L"https://"))
    title.clear();

  static const std::set<std::wstring> common_titles{
    L"Blank Page",            // Internet Explorer
    L"InPrivate",             // Internet Explorer
    L"New Tab",               // Chrome, Firefox
    L"Private Browsing",      // Firefox
    L"Private browsing",      // Opera
    L"Problem loading page",  // Firefox
    L"Speed Dial",            // Opera
    L"Untitled",              // Chrome
  };
  if (common_titles.count(title))
    title.clear();

  static const std::vector<std::wstring> common_suffixes{
    L" - Network error",  // Chrome
  };
  for (const auto& suffix : common_suffixes) {
    if (EndsWith(title, suffix)) {
      title.clear();
      return;
    }
  }
}

void RemoveCommonWebBrowserAffixes(std::wstring& title) {
  static const std::vector<std::wstring> common_suffixes{
    L" - Audio playing",  // Chrome
  };
  for (const auto& suffix : common_suffixes) {
    EraseRight(title, suffix);
  }
}

void NormalizeWebBrowserTitle(const std::wstring& url, std::wstring& title) {
  IgnoreCommonWebBrowserTitles(url, title);
  RemoveCommonWebBrowserAffixes(title);
}

}  // namespace recognition
}  // namespace track
