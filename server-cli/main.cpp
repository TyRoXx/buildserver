#include <boost/program_options.hpp>
#include <iostream>

int main(int argc, char **argv)
{
	boost::program_options::options_description desc("Allowed options");
	desc.add_options()("help", "produce help message");

	boost::program_options::positional_options_description positional;
	boost::program_options::variables_map vm;
	try
	{
		boost::program_options::store(
		    boost::program_options::command_line_parser(argc, argv).options(desc).positional(positional).run(), vm);
	}
	catch (boost::program_options::error const &ex)
	{
		std::cerr << ex.what() << '\n' << desc << "\n";
		return 1;
	}

	boost::program_options::notify(vm);

	if (vm.count("help"))
	{
		std::cerr << desc << "\n";
		return 1;
	}
}
