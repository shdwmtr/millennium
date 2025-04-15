import json
import os
from pathlib import Path
import shutil
import requests
from typing import List, Dict, Optional
import zipfile
from util.logger import logger

plugin_updater_cache = None

class PluginUpdater:
    def __init__(self):
        self.plugins_path = os.getenv("MILLENNIUM__PLUGINS_PATH")
        if not self.plugins_path:
            raise EnvironmentError("MILLENNIUM__PLUGINS_PATH environment variable not set")

    def _read_metadata(self, plugin_path: Path) -> Optional[Dict[str, str]]:
        """Read metadata.json from a plugin directory if it exists."""
        metadata_path = plugin_path / "metadata.json"
        if not metadata_path.exists():
            return None

        try:
            with open(metadata_path, 'r') as f:
                metadata = json.load(f)
                if "id" in metadata and "commit" in metadata:
                    return {
                        "id": metadata["id"],
                        "commit": metadata["commit"],
                        "name": plugin_path.name
                    }
        except (json.JSONDecodeError, IOError) as e:
            print(f"Failed to read metadata.json for plugin {plugin_path.name}: {e}")
            return None

    def _get_plugin_data(self) -> List[Dict[str, str]]:
        """Get plugin data from all plugins that have metadata.json."""
        plugin_data = []
        
        for plugin_dir in Path(self.plugins_path).iterdir():
            if not plugin_dir.is_dir():
                continue
                
            metadata = self._read_metadata(plugin_dir)
            if metadata:
                plugin_data.append(metadata)
                
        return plugin_data

    def extract_zip(self, zip_path: str, destination: str) -> bool:
        """Extract a zip file to the specified destination."""
        try:
            with zipfile.ZipFile(zip_path, 'r') as zip_ref:
                zip_ref.extractall(destination)
            return True
        except zipfile.BadZipFile as e:
            print(f"Failed to extract {zip_path}: {e}")
            return False

    def download_plugin_update(self, id: str, name: str) -> bool:
        """Download and update a plugin directly into the plugins path."""
        try:
            url = f"https://steambrew.app/api/v1/plugins/download?id={id}&n={name}.zip"
            logger.log(f"Downloading plugin {name} from {url}")

            response = requests.get(url, stream=True)
            response.raise_for_status()
            logger.log(f"Downloaded plugin {name} from {url}")

            # Create temp directory
            temp_dir = Path(self.plugins_path) / "__tmp_extract"
            temp_dir.mkdir(parents=True, exist_ok=True)

            temp_zip_path = temp_dir / f"{name}.zip"
            with open(temp_zip_path, 'wb') as f:
                for chunk in response.iter_content(chunk_size=8192):
                    f.write(chunk)

            logger.log(f"Extracting plugin {name} into {temp_dir}")

            # Extract zip
            if not self.extract_zip(temp_zip_path, temp_dir):
                shutil.rmtree(temp_dir)
                return False

            # Detect the extracted folder (assume only one directory is created)
            extracted_dirs = [d for d in temp_dir.iterdir() if d.is_dir()]
            if not extracted_dirs:
                logger.log(f"No directory found in extracted plugin {name}")
                shutil.rmtree(temp_dir)
                return False

            extracted_folder = extracted_dirs[0]
            target_folder = Path(self.plugins_path) / name

            # Replace existing plugin folder
            if target_folder.exists():
                shutil.rmtree(target_folder)
            extracted_folder.rename(target_folder)

            logger.log(f"Plugin {name} installed successfully to {target_folder}")

            # Clean up temp dir
            shutil.rmtree(temp_dir, ignore_errors=True)
            return True

        except Exception as e:
            logger.log(f"Failed to update plugin {name}: {e}")
            return False

    def get_request_body(self):
        return self._get_plugin_data()
