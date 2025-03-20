///////////////////////////////////////////////////////////////////////////////
// Dependencies
///////////////////////////////////////////////////////////////////////////////
#include "Core/Core.hpp"
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>
#include <imgui.h>
#include <imgui-SFML.h>

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    if (argc == 1) {
        std::cout << "Usage: " << argv[0] << " <file>" << std::endl;
        return (0);
    }

    Moon::VideoPlayer player(argv[1]);

    bool isFullscreen = false;
    sf::Vector2i lastPosition;

    sf::RenderWindow window(sf::VideoMode({800, 600}), "Moon", sf::Style::Default);
    sf::Clock clock;
    // window.setFramerateLimit(60);

    if (!ImGui::SFML::Init(window)) {
        std::cerr << "Coudl'nt initialize ImGui" << std::endl;
        return (EXIT_FAILURE);
    }

    player.Play();

    sf::Sprite sprite(player.GetCurrentFrameTexture());

    while (window.isOpen()) {
        while (auto event = window.pollEvent()) {
            ImGui::SFML::ProcessEvent(window, *event);

            if (event->is<sf::Event::Closed>()) {
                window.close();
                player.Stop();
            } else if (auto size = event->getIf<sf::Event::Resized>()) {
                window.setView(sf::View(sf::FloatRect(
                    {0.f, 0.f},
                    {
                        static_cast<float>(size->size.x),
                        static_cast<float>(size->size.y)
                    }
                )));

                sf::Vector2u videoSize = player.GetCurrentFrameTexture().getSize();
                float scaleX = static_cast<float>(size->size.x) / videoSize.x;
                float scaleY = static_cast<float>(size->size.y) / videoSize.y;
                float scale = std::min(scaleX, scaleY);

                sprite.setScale({scale, scale});
                sprite.setPosition({
                    (size->size.x - videoSize.x * scale) / 2,
                    (size->size.y - videoSize.y * scale) / 2}
                );
            } else if (auto key = event->getIf<sf::Event::KeyPressed>()) {
                if (key->code == sf::Keyboard::Key::Space) {
                    player.TogglePause();
                } else if (key->code == sf::Keyboard::Key::F) {
                    isFullscreen = !isFullscreen;
                    window.create(sf::VideoMode({800, 600}), "Moon", sf::Style::Default, (isFullscreen ? sf::State::Fullscreen : sf::State::Windowed));
                    window.setPosition(lastPosition);
                }
            }
        }

        ImGui::SFML::Update(window, clock.restart());

        if (!isFullscreen) {
            lastPosition = window.getPosition();
        }
        player.Update();

        ImGui::Begin("Controls");
        if (ImGui::Button("Play/Pause")) {
            player.TogglePause();
        }

        // Add playback speed controls
        float speed = static_cast<float>(player.GetPlaybackSpeed());
        if (ImGui::SliderFloat("Speed", &speed, 0.25f, 4.0f, "%.2fx")) {
            player.SetPlaybackSpeed(static_cast<double>(speed));
        }

        // Add quick preset buttons for playback speed
        ImGui::Text("Presets:");
        ImGui::SameLine();
        if (ImGui::Button("0.5x")) {
            player.SetPlaybackSpeed(0.5);
        }
        ImGui::SameLine();
        if (ImGui::Button("1.0x")) {
            player.SetPlaybackSpeed(1.0);
        }
        ImGui::SameLine();
        if (ImGui::Button("1.5x")) {
            player.SetPlaybackSpeed(1.5);
        }
        ImGui::SameLine();
        if (ImGui::Button("2.0x")) {
            player.SetPlaybackSpeed(2.0);
        }

        ImGui::Text("%.0f/%.0f", player.GetCurrentTime(), player.GetDuration());
        ImGui::End();

        window.clear(sf::Color::Black);

        sprite.setTexture(player.GetCurrentFrameTexture());
        window.draw(sprite);
        ImGui::SFML::Render(window);

        window.display();
    }

    ImGui::SFML::Shutdown();

    return (0);
}
