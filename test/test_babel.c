#include <babel.h>

void print_file_format (BabelFile *file, gconstpointer user_data)
{
	printf("%s : %d%d%d%d%d%d\n",
		file->label,
		file->mode.waypointsRead, file->mode.waypointsWrite,
		file->mode.tracksRead, file->mode.tracksWrite,
		file->mode.routesRead, file->mode.routesWrite);
}

int main(int argc, char*argv[])
{
	a_babel_init();

	if (argc != 7) return 1;
	BabelMode mode = { atoi(argv[1]),atoi(argv[2]),atoi(argv[3]),atoi(argv[4]),atoi(argv[5]),atoi(argv[6]) };
	a_babel_foreach_file_with_mode(mode, print_file_format, NULL);

	a_babel_uninit();

	return 0;
}

