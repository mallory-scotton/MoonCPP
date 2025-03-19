///////////////////////////////////////////////////////////////////////////////
// Dependencies
///////////////////////////////////////////////////////////////////////////////
#include "Core/Core.hpp"
#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <SFML/System.hpp>

///////////////////////////////////////////////////////////////////////////////
int main(int argc, char* argv[])
{
    if (argc == 1) {
        std::cout << "Usage: " << argv[0] << " <file>" << std::endl;
        return (0);
    }

    Moon::VideoPlayer player(argv[1]);

    sf::RenderWindow window(sf::VideoMode({800, 600}), "Moon", sf::Style::Default);
    window.setFramerateLimit(60);

    player.Play();

    sf::Sprite sprite(player.GetCurrentFrameTexture());

    while (window.isOpen()) {
        while (auto event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            }
            if (auto size = event->getIf<sf::Event::Resized>()) {
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
            }
        }

        player.Update();

        window.clear(sf::Color::Black);
        sprite.setTexture(player.GetCurrentFrameTexture());
        window.draw(sprite);
        window.display();
    }

    return (0);
}
