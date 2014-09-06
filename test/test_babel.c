// Decide the Babel file types capability you wish to list
// e.g. for read support of waypoints, tracks and routes:
// run like: ./test_babel 1 0 1 0 1 0
#include <stdlib.h>
#include <babel.h>

static void print_file_format (gpointer data, gpointer user_data)
{
	BabelFile *file = (BabelFile*)data;
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

