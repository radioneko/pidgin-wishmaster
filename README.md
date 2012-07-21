## pidgin-wishmaster - Pidgin nick autocomplete plugin for MUCs

### Description
This plugin is intended to make nick autocompletion in multi
user chats easier. With pidgin-wishmaster you can
* insert nick by left-clicking it in participants list or in chat log window
* insert last clicked nick with TAB key (works only when cursor is located
at the beginning of the message)

### Installation
You will need pidgin development package (pidgin-devel or pidgin&#95;dev)
    git clone git://github.com/radioneko/pidgin-wishmaster.git
	cd pidgin-wishmaster
	./autogen.sh
	./configure
	make
If you want to install it globally, then execute `make install` as root.
To install locally do
	mkdir -p ~/.purple/plugins/
	cp src/.libs/pidgin-wishmaster.so ~/.purple/plugins/
