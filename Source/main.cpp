#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <conio.h>

#include "app/config/config_loader.h"
#include "app/steam/steam_client.h"
#include "app/process/process_manager.h"

using namespace app::config;
using namespace app::steam;
using namespace app::process;

int run_child(int app_id)
{
    SteamContext steam_context;
    steam_context.set_app_id(std::to_string(app_id));

    if (!steam_context.init())
    {
        std::cerr << "[Child " << app_id << "] Failed to initialize SteamAPI. Is Steam running?" << std::endl;
        return 1;
    }

    std::cout << "[Child " << app_id << "] Idling game..." << std::endl;

    while (true)
    {
        steam_context.run_callbacks();
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}

int main(int argc, char* argv[])
{
    if (argc == 3 && std::string(argv[1]) == "--child")
    {
        const int app_id{ std::stoi(argv[2]) };
        return run_child(app_id);
    }

    std::cout << "Steam Idler Manager" << std::endl;
    std::cout << "===================" << std::endl;

    SteamContext steam_context;
    // 1. Initialize SteamAPI with a generic ID (Spacewar - 480) to check ownership
    steam_context.set_app_id("480");

    if (!steam_context.init())
    {
        std::cerr << "Fatal Error: Failed to initialize SteamAPI. Please ensure Steam is running and you are logged in." << std::endl;
        std::cerr << "Also ensure steam_api64.dll is in the same folder as this executable." << std::endl;
        std::cout << "Press Enter to exit...";
        std::cin.get();
        return 1;
    }

    const auto game_ids{ ConfigLoader::load_game_ids("games.cfg") };
    if (game_ids.empty())
    {
        std::cout << "No game IDs found in games.cfg." << std::endl;
        steam_context.shutdown();
        std::cout << "Press Enter to exit...";
        std::cin.get();
        return 0;
    }

    ProcessManager process_manager;

    while (true)
    {
        int count{ 0 };

        for (const int app_id : game_ids)
        {
            if (steam_context.is_app_installed(app_id))
            {
                std::cout << "Verified ownership of AppID " << app_id << ". Launching idler..." << std::endl;
                if (process_manager.launch_child(app_id))
                {
                    count++;
                }
                else
                {
                    std::cerr << "Failed to launch child process for " << app_id << std::endl;
                }
            }
            else
            {
                std::cout << "Skipping AppID " << app_id << " (Not owned)" << std::endl;
            }
        }

        if (count == 0)
        {
            std::cout << "No valid games to idle." << std::endl;
            break;
        }
        else
        {
            std::cout << "\nIdling " << count << " games." << std::endl;
            std::cout << "Restarting in 3 hours. Press any key to stop all idlers and exit." << std::endl;

            bool stop{ false };
            // 3 hours = 10800 seconds
            for (int i{ 0 }; i < 10800; ++i)
            {
                if (_kbhit())
                {
                    stop = true;
                    _getch(); // consume the key
                    break;
                }
                std::this_thread::sleep_for(std::chrono::seconds(1));
            }

            std::cout << (stop ? "Stopping idlers..." : "Restarting idlers...") << std::endl;
            process_manager.terminate_all();

            if (stop)
            {
                break;
            }
        }
    }

    steam_context.shutdown();
    return 0;
}
