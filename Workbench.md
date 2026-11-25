# Workbench installation (Ubuntu 22.04 LTS – in WSL)
Start WSL session:
```
$ wsl ~
```

In WSL, setup flatpak utility:
```
$ sudo apt install flatpak gnome-software-plugin-flatpak -y
$ sudo flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo
$ exit
```

From Windows host, restart WSL – run:
```
$ wsl --terminate Ubuntu-22.04
$ wsl ~
```

In WSL, install Workbench:
```
$ sudo flatpak install flathub re.sonny.Workbench -y
```

Run Workbench:
```
$ flatpak run re.sonny.Workbench
```
