To run the functional tests made with cucumber you need:

* Ruby >= 1.9
* The "bundler" gem (once you've installed Ruby you can run "gem install bundler")
* Docker >= 1.0 (installed so that you can use it without sudo)

On Ubuntu 14.04 you can install all this with these commands:

    sudo apt-get install ruby ruby-dev
    sudo gem install bundler --no-rdoc --no-ri

    sudo apt-key adv --keyserver hkp://keyserver.ubuntu.com:80 --recv-keys 36A1D7869245C8950F966E92D8576A8BA88D21E9
    sudo sh -c "echo deb https://get.docker.io/ubuntu docker main > /etc/apt/sources.list.d/docker.list"
    sudo apt-get update
    sudo apt-get install lxc-docker
    sudo gpasswd -a ${USER} docker
    sudo chown ${USER} /var/run/docker.sock

To prepare all the containers, just run:

    src/test/containers/init -j4

It will build the containers and install the Ruby gems with bundler. If anything fails, ask for support.

Then to run the tests, run this:

    src/test/containers/make -j4 && src/test/cucumber

You can adjust the -j flag appropriately.


If you changed blockchain properties and must restart it, run this to rebuild the seed image and the sample network node images:

    src/test/containers/build_seed
    src/test/containers/build_net

When you run the tests, the containers are not removed so that you can inspect them.

You can use src/test/containers/show_log to display the log of the last container that used a specific image.

When you're done working, you should remove all containers with:

    src/test/containers/remove_bcexchange_containers

It will also clean the shared temporary directories in `src/test/containers/tmp`.


Testing with old versions
-------------------------

To use an old version in the tests, you first need to build a binary from this version. Checkout the appropriate tag on a detached head. For example:

    git checkout v0.4.5

Build and strip it:

    src/test/containers/make clean
    src/test/containers/make -j4 DEBUGFLAGS=
    strip src/bcexchanged

Put the binary on a public URL.

Copy `test/containers/old/0.4.2` to a new directory and update the Dockerfile to retrieve the new binary.

Update `test/containers/build_base` to also build your version and run it.

Add the version to the `OLD_VERSION` array in `test/containers/build_net` and run it.

You should be able to use steps like this:

    Given a node "Old node" running version "0.4.5"
