#include <Text/Parser.hpp>
#include <stupid_logger.hpp>
#include <utils.hpp>

#include <d3dx9core.h>

#include <algorithm>
#include <functional>

namespace Text {

bool icase_starts_with(std::wstring_view text, std::wstring_view prefix)
{
    if (text.length() < prefix.length())
    {
        return false;
    }

    for (size_t i = 0; i < prefix.length(); ++i)
    {
        if (towlower(text[i]) != towlower(prefix[i]))
        {
            return false;
        }
    }

    return true;
}

size_t icase_find(std::wstring_view text, std::wstring_view prefix)
{
    auto it =
        std::search(
            text.begin(), text.end(),
            prefix.begin(), prefix.end(),
            [](auto a, auto b) { return towlower(a) == towlower(b); }
        );

    if (it == text.end())
    {
        return std::wstring::npos;
    }

    return std::distance(text.begin(), it);
}

D3DCOLOR GetColorFromTag(std::wstring_view text, D3DCOLOR defaultColor)
{
    if (icase_starts_with(text, COLOR_TAG_START))
    {
        try
        {
            const size_t rEnd = text.find(L',', 7);
            const size_t gEnd = text.find(L',', rEnd + 1);

            std::size_t pos{};

            const int a = 255;
            const int r = std::stoi(&text[7], &pos);
            const int g = std::stoi(&text[rEnd + 1], &pos);
            const int b = std::stoi(&text[gEnd + 1], &pos);

            return a << 24 | r << 16 | g << 8 | b;
        }
        catch (const std::exception& e)
        {
            lgr.error("Failed to parse color from text: {}")(utils::from_wstring(std::wstring{text}));
        }
    }
    return defaultColor;
}

std::vector<Token> parse_tokens(std::wstring_view str, Font& font)
{
    std::vector<Token> result;

    const std::function<void(std::wstring_view str)> processWord = [&result, &processWord](std::wstring_view str){
        size_t pos = icase_find(str, COLOR_TAG_START);
        if (pos == std::wstring::npos)
        {
            // no color tag
            result.emplace_back(str);
            return;
        }

        if (pos != 0) // color not from the str beginning
        {
            result.emplace_back(str.substr(0, pos));
            str.remove_prefix(pos);
        }

        pos = icase_find(str, COLOR_TAG_END);
        if (pos == std::wstring::npos)
        {
            // no color end tag
            result.emplace_back(str);
            return;
        }

        result.emplace_back(str.substr(0, pos + COLOR_TAG_END.length())); // TODO: use contants
        str.remove_prefix(pos + COLOR_TAG_END.length());

        if (!str.empty())
        {
            processWord(str);
        }
    };

    size_t pos = 0;
    while((pos = str.find_first_of(L" \r"), pos) != std::string::npos)
    {
        if (str[pos] == L' ') // space - just split words
        {
            processWord(str.substr(0, pos));
            result.emplace_back(L" ");
            str.remove_prefix(pos + 1);
        }
        else if(str[pos] == L'\r') // new line
        {
            processWord(str.substr(0, pos));
            result.emplace_back(L"\r\n");
            str.remove_prefix(pos + 2);
        }
    }

    processWord(str);

    bool in_color_tag = false;
    uint32_t color = 0;
    for (auto& token : result)
    {
        if (token.text == L" " || token.text == L"\r\n")
        {
            continue;
        }

        std::wstring_view text = token.text;

        if (!in_color_tag)
        {
            color = token.color;
        }

        if (icase_starts_with(text, COLOR_TAG_START))
        {
            in_color_tag = true;
            color = GetColorFromTag(text, token.color);
            text.remove_prefix(text.find(L">") + 1);
        }

        auto pos = icase_find(text, COLOR_TAG_END);
        if (pos != std::wstring::npos)
        {
            in_color_tag = false;
            text.remove_suffix(text.length() - pos);
        }

        token.text  = text;
        token.color = color;
        token.width = font.CalcTextWidth(text);
    }

    return result;
}

size_t calc_lines(const std::vector<Token>& text, Font& font, const RECT &rect)
{
    const size_t line_width = rect.right - rect.left;

    size_t lines = 1;
    size_t cur_width = 0;
    for (auto& token : text)
    {
        if (token.text == L"\r\n")
        {
            cur_width = 0;
            lines++;
        }
        else if (token.text == L" ")
        {
            if (cur_width != 0) // if not a new line
            {
                cur_width += font.GetSpaceWidth();
            }
        }
        else
        {
            if (cur_width + token.width < line_width)
            {
                cur_width += token.width;
            }
            else
            {
                cur_width = 0;
                lines++;
            }
        }
    }

    return lines;
}

} // namespace Text