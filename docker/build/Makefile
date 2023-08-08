all: \
	ubuntu2004-gtk3 \
	ubuntu2004-gtk2 \
	ubuntu2204-gtk3 \
	ubuntu2204-gtk2

ubuntu2004-gtk3:
	docker build --build-arg UBUNTU_VERSION=20.04 --build-arg LIBGTK=libgtk-3-dev -t viking-build:$@ .

ubuntu2004-gtk2:
	docker build --build-arg UBUNTU_VERSION=20.04 --build-arg LIBGTK=libgtk2.0-dev -t viking-build:$@ .

ubuntu2204-gtk3:
	docker build --build-arg UBUNTU_VERSION=22.04 --build-arg LIBGTK=libgtk-3-dev -t viking-build:$@ .

ubuntu2204-gtk2:
	docker build --build-arg UBUNTU_VERSION=22.04 --build-arg LIBGTK=libgtk2.0-dev -t viking-build:$@ .
	