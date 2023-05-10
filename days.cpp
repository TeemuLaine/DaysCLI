#include <iostream>    // for standard I/O streams
#include <iomanip>     // for stream control
#include <string>      // for std::string class
#include <cstdlib>     // for std::getenv
#include <chrono>      // for the std::chrono facilities
#include <sstream>     // for std::stringstream class
#include <vector>      // for std::vector class
#include <optional>    // for std::optional
#include <string_view> // for std::string_view
#include <filesystem>  // for path utilities
#include <memory>      // for smart pointers

#include "event.h"    // for our Event class
#include "rapidcsv.h" // for the header-only library RapidCSV

// Parses the string `buf` for a date in YYYY-MM-DD format. If `buf` can be parsed,
// returns a wrapped `std::chrono::year_month_day` instances, otherwise `std::nullopt`.
// NOTE: Once clang++ and g++ implement chrono::from_stream, this could be replaced by
// something like this:
//  chrono::year_month_day birthdate;
//  std::istringstream bds{birthdateValue};
//  std::basic_istream<char> stream{bds.rdbuf()};
//  chrono::from_stream(stream, "%F", birthdate);
// However, I don't know how errors should be handled. Maybe this function could then
// continue to serve as a wrapper.
std::optional<std::chrono::year_month_day> getDateFromString(const std::string &buf)
{
    using namespace std; // use std facilities without prefix inside this function

    constexpr string_view yyyymmdd = "YYYY-MM-DD";
    if (buf.size() != yyyymmdd.size())
    {
        return nullopt;
    }

    istringstream input(buf);
    string part;
    vector<string> parts;
    while (getline(input, part, '-'))
    {
        parts.push_back(part);
    }
    if (parts.size() != 3)
    { // expecting three components, year-month-day
        return nullopt;
    }

    int year{0};
    unsigned int month{0};
    unsigned int day{0};
    try
    {
        year = stoul(parts.at(0));
        month = stoi(parts.at(1));
        day = stoi(parts.at(2));

        auto result = chrono::year_month_day{
            chrono::year{year},
            chrono::month(month),
            chrono::day(day)};

        if (result.ok())
        {
            return result;
        }
        else
        {
            return nullopt;
        }
    }
    catch (invalid_argument const &ex)
    {
        cerr << "conversion error: " << ex.what() << endl;
    }
    catch (out_of_range const &ex)
    {
        cerr << "conversion error: " << ex.what() << endl;
    }

    return nullopt;
}

// Returns the value of the environment variable `name` as an `std::optional``
// value. If the variable exists, the value is a wrapped `std::string`,
// otherwise `std::nullopt`.
std::optional<std::string> getEnvironmentVariable(const std::string &name)
{
    const char *value = std::getenv(const_cast<char *>(name.c_str()));
    if (nullptr != value)
    {
        std::string valueString = value;
        return valueString;
    }
    return std::nullopt;
}

// Returns `date` as a string in `YYYY-MM-DD` format.
// The ostream support for `std::chrono::year_month_day` is not
// available in most (any?) compilers, so we roll our own.
std::string getStringFromDate(const std::chrono::year_month_day &date)
{
    std::ostringstream result;

    result
        << std::setfill('0') << std::setw(4) << static_cast<int>(date.year())
        << "-" << std::setfill('0') << std::setw(2) << static_cast<unsigned>(date.month())
        << "-" << std::setfill('0') << std::setw(2) << static_cast<unsigned>(date.day());

    return result.str();
}

// Print `T` to standard output.
// `T` needs to have an overloaded << operator.
template <typename T>
void display(const T &value)
{
    std::cout << value;
}

// Prints a newline to standard output.
inline void newline()
{
    std::cout << std::endl;
}

// Overload the << operator for the Event class.
// See https://learn.microsoft.com/en-us/cpp/standard-library/overloading-the-output-operator-for-your-own-classes?view=msvc-170
std::ostream &operator<<(std::ostream &os, const Event &event)
{
    os
        << getStringFromDate(event.getTimestamp()) << ": "
        << event.getDescription()
        << " (" + event.getCategory() + ")";
    return os;
}

// Gets the number of days betweem to time points.
int getNumberOfDaysBetween(std::chrono::sys_days const &earlier, std::chrono::sys_days const &later)
{
    return (later - earlier).count();
}

std::string getHomeDirectory()
{
    auto homeString = getEnvironmentVariable("HOME");
    if (!homeString.has_value())
    {
        // HOME not found, maybe this is Windows? Try USERPROFILE.
        auto userProfileString = getEnvironmentVariable("USERPROFILE");
        if (!userProfileString.has_value())
        {
            std::cerr << "Unable to determine home directory";
            return "";
        }
        else
        {
            return userProfileString.value();
        }
    }
    else
    {
        return homeString.value();
    }
    return "";
}

int main(int argc, char *argv[])
{
    using namespace std;

    // Get the current date from the system clock and extract year_month_day.
    // See https://en.cppreference.com/w/cpp/chrono/year_month_day
    const chrono::time_point now = chrono::system_clock::now();
    const chrono::year_month_day currentDate{chrono::floor<chrono::days>(now)};

    // Note that you can't print an `std::chrono::year_month_day`
    // with `display()` because there is no overloaded << operator
    // for it (yet).

    // Construct a path for the events file.
    // If the user's home directory can't be determined, give up.
    std::string homeDirectoryString = getHomeDirectory();
    if (homeDirectoryString == "")
        return 1;

    // Using ternary operators variables can be assigned with argv[] values depending on the value of
    // argc without repeating if statements. Default assignment is an empty string
    std::string command = (argc > 1) ? std::string(argv[1]) : "";
    std::string option1 = (argc > 2) ? std::string(argv[2]) : "";
    std::string parameter1 = (argc > 3) ? std::string(argv[3]) : "";
    std::string option2 = (argc > 4) ? std::string(argv[4]) : "";
    std::string parameter2 = (argc > 5) ? std::string(argv[5]) : "";

    namespace fs = std::filesystem; // save a little typing
    fs::path daysPath{homeDirectoryString};
    daysPath /= ".days"; // append our own directory
    if (!fs::exists(daysPath))
    {
        display(daysPath.string());
        display(" does not exist, please create it");
        newline();
        return 1; // nothing to do anymore, exit program

        // To create the directory:
        // std::filesystem::create_directory(daysPath);
        // See issue: https://github.com/jerekapyaho/days_cpp/issues/4
    }

    // Now we should have a valid path to the `~/.days` directory.
    // Construct a pathname for the `events.csv` file.
    auto eventsPath = daysPath / "events.csv";

    //
    // Read in the CSV file from `eventsPath` using RapidCSV
    // See https://github.com/d99kris/rapidcsv
    //
    rapidcsv::Document document{eventsPath.string()};
    vector<string> dateStrings{document.GetColumn<string>("date")};
    vector<string> categoryStrings{document.GetColumn<string>("category")};
    vector<string> descriptionStrings{document.GetColumn<string>("description")};

    vector<Event> events;
    for (size_t i{0}; i < dateStrings.size(); i++)
    {
        auto date = getDateFromString(dateStrings.at(i));
        if (!date.has_value())
        {
            cerr << "bad date at row " << i << ": " << dateStrings.at(i) << '\n';
            continue;
        }

        Event event{
            date.value(),
            categoryStrings.at(i),
            descriptionStrings.at(i)};
        events.push_back(event);
    }

    const auto today = chrono::sys_days{
        floor<chrono::days>(chrono::system_clock::now())};

    if (argc > 1)
    {
        if (command == "list")
        {
            for (auto &event : events)
            {
                const auto delta = (chrono::sys_days{event.getTimestamp()} - today).count();
                if (argc > 2)
                {
                    if (option1 == "--today" && delta != 0)
                        continue; // if both are true, skip one iteration, otherwise keep going.

                    else if (option1 == "--before-date")
                    {
                        if (argc > 3 && argc != 5)
                        {
                            if (argc == 6 && option2 == "--after-date" && getDateFromString(parameter2) > event.getTimestamp())
                                continue;

                            else if (getDateFromString(parameter1) <= event.getTimestamp())
                            {
                                continue;
                            }
                        }
                        else
                        {
                            std::cout << "Missing date." << std::endl;
                            break;
                        }
                    }
                    else if (option1 == "--after-date")
                    {
                        if (argc > 3)
                        {
                            if (getDateFromString(parameter1) > event.getTimestamp())
                                continue;
                        }
                        else
                        {
                            std::cout << "Missing date." << std::endl;
                            break;
                        }
                    }
                    else if (option1 == "--date")
                    {
                        if (getDateFromString(parameter1) != event.getTimestamp())
                            continue;
                    }
                    else if (option1 == "--categories")
                    {
                        bool multipleCategories = (parameter1.find(',') != std::string::npos) ? true : false;
                        if (!multipleCategories)
                        {
                            if ((parameter1 != event.getCategory() && option2 != "--exclude") || (option2 == "--exclude" && parameter1 == event.getCategory()))
                            {
                                continue;
                            }
                        }
                        else
                        {
                            std::string category;
                            std::stringstream catStream(parameter1);
                            bool categoryFound = false;

                            // Separate with getline and look for a match
                            while (std::getline(catStream, category, ','))
                            {
                                if (category == event.getCategory())
                                    categoryFound = true;
                            }
                            if ((option2 != "--exclude" && !categoryFound) || (option2 == "--exclude" && categoryFound))
                                continue;
                        }
                    }
                    else if (option1 == "--no-category")
                    {
                        if (event.getCategory() != "")
                            continue;
                    }
                }

                ostringstream line;
                line << event << " - ";

                if (delta < 0)
                {
                    line << abs(delta) << " days ago";
                }
                else if (delta > 0)
                {
                    line << "in " << delta << " days";
                }
                else
                {
                    line << "today";
                }

                display(line.str());
                newline();
            }
        }
        else if (command == "add" && (argc == 6 || argc == 8))
        {
            std::ofstream file(eventsPath, std::ios::app);
            if (option1 == "--category" && argc == 6 && option2 == "--description")
            {
                file << getStringFromDate(today) << "," << parameter1 << "," << parameter2 << "\n";
            }
            else if (option1 == "--date" && option2 == "--category" && std::string(argv[6]) == "--description")
            {
                std::string parameter3 = std::string(argv[7]);
                file << parameter1 << "," << parameter2 << "," << parameter3 << "\n";
            }
            else
                std::cout << "Invalid options" << std::endl;
            file.close();
        }
        else if (command == "delete" && argc > 2)
        {
            std::fstream file(eventsPath);
            std::string tempFilePath = homeDirectoryString + "/.days/tempFile.csv";
            std::ofstream tempFile(tempFilePath);
            std::string text;
            while (std::getline(file, text))
            {
                if (option1 == "--date" && text.find(parameter1) != std::string::npos)
                {
                    continue;
                }
                tempFile << text << std::endl;
            }
            file.close();
            tempFile.close();
            if (std::string(argv[argc - 1]) != "--dry-run")
            {
                std::remove(eventsPath.c_str());
                std::rename(tempFilePath.c_str(), eventsPath.c_str());
            }
            else
            {
                std::cout << "Dry run, would delete:" << std::endl;
                std::remove(tempFilePath.c_str());
            }
        }
        else
            std::cout << "Invalid command." << std::endl;
    }
    return 0;
}
