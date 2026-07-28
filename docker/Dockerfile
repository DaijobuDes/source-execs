FROM cm2network/steamcmd:root-bookworm

ENV USER=steam
ENV HOMEDIR="/home/${USER}"
ENV STEAMCMDDIR="${HOMEDIR}/steamcmd"

RUN dpkg --add-architecture i386 && apt update && apt install libncurses5:i386 -y && apt clean

WORKDIR ${STEAMCMDDIR}

USER ${USER}
