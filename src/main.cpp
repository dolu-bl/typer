#include <fstream>
#include <iostream>
#include <string>

#include <boost/program_options.hpp>

#include "typer.hpp"

namespace po = boost::program_options;

void printHelp(const po::options_description& options)
{
    std::cout
        << "Использование: typer [ПАРАМЕТР]…\n"
        << "Утилита для печати текстового файла с клавиатуры.\n\n"
        << options << "\n\n"
        << "Пример:\n  ./typer --source=text_file.txt\n";
}

int main(int argc, char* argv[])
{
    try
    {
        po::options_description options("Допустимые параметры");
        options.add_options()(
            "help,h",
            "вывести справочное сообщение"
        )(
            "source,s",
            po::value<std::string>(),
            "путь к текстовому файлу"
        )(
            "reset,r",
            "сбросить прогресс для данного файла"
        );

        po::variables_map vm;
        try
        {
            po::store(po::parse_command_line(argc, argv, options), vm);
            po::notify(vm);
        }
        catch (const po::error& e)
        {
            std::cout << "Ошибка: " << e.what() << "\n";
            printHelp(options);
            return EXIT_FAILURE;
        }

        if (vm.count("help") || argc == 1)
        {
            printHelp(options);
            return EXIT_SUCCESS;
        }

        if (!vm.count("source"))
        {
            std::cout << "Ошибка: не указан файл --source\n";
            printHelp(options);
            return EXIT_FAILURE;
        }

        const std::string filepath = vm["source"].as<std::string>();
        const bool reset = vm.count("reset") > 0;

        std::ifstream in(filepath);
        if (!in.is_open())
        {
            std::cout << "Ошибка: не удалось открыть файл " << filepath << "\n";
            return EXIT_FAILURE;
        }
        const std::string content(
            (std::istreambuf_iterator<char>(in)),
            std::istreambuf_iterator<char>()
        );

        size_t offset = 0;
        Typer typer;
        typer.run(filepath, content, offset, reset);

        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        std::cout << "Ошибка: " << e.what() << "\n";
        return EXIT_FAILURE;
    }
}
