all: src/memthick.c src/wdcalc.c src/wdmap.c src/leafthick.c
	make memthick groan=${groan}
	make wdcalc groan=${groan}
	make wdmap groan=${groan}
	make ĺeafthick groan=${groan}

memthick: src/memthick.c
	gcc src/memthick.c -I$(groan) -L$(groan) -D_POSIX_C_SOURCE=200809L -o memthick -lgroan -lm -std=c99 -pedantic -Wall -Wextra -O3 -march=native

wdcalc: src/wdcalc.c
	gcc src/wdcalc.c -I$(groan) -L$(groan) -D_POSIX_C_SOURCE=200809L -o wdcalc -lgroan -lm -std=c99 -pedantic -Wall -Wextra -O3 -march=native

wdmap: src/wdmap.c
	gcc src/wdmap.c -I$(groan) -L$(groan) -D_POSIX_C_SOURCE=200809L -o wdmap -lgroan -lm -std=c99 -pedantic -Wall -Wextra -O3 -march=native

leafthick: src/leafthick.c
	gcc src/leafthick.c -I$(groan) -L$(groan) -D_POSIX_C_SOURCE=200809L -o leafthick -lgroan -lm -std=c99 -pedantic -Wall -Wextra -O3 -march=native

install:
	if [ -f memthick ];  then cp memthick ${HOME}/.local/bin;  fi
	if [ -f wdcalc ];    then cp wdcalc ${HOME}/.local/bin;    fi
	if [ -f wdmap ];     then cp wdmap ${HOME}/.local/bin;     fi
	if [ -f leafthick ]; then cp leafthick ${HOME}/.local/bin; fi
