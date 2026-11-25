import os
import gzip
import xml.etree.ElementTree as ET
import requests


def get_appstream_version(
    app_id: str,
    repo_url: str = "https://flatpak.orionbrowser.com/repo/beta/",
    arch: str = "x86_64",
):

  appstream_url = os.path.join(repo_url, "appstream", arch, "appstream.xml.gz")

  response = requests.get(appstream_url, timeout=20)
  response.raise_for_status()
    
  xml_data = gzip.decompress(response.content).decode("UTF-8")
    
  root = ET.fromstring(xml_data)
  release = root.find(f"component/id[.='{app_id}']/../releases/release")
 
  return release.get("version") if release is not None else None


print("com.kagi.Orion latest version is", get_appstream_version("com.kagi.Orion"))

import gi
gi.require_version('Gtk', '4.0')
from gi.repository import Gtk

appstream = Gtk.UriLauncher(uri="appstream:com.kagi.Orion")
appstream.launch()
