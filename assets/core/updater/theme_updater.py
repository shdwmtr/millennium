import Millennium  # type: ignore
import os, json, shutil, time, requests, arrow, platform
from datetime import datetime
from api.themes import find_all_themes
from api.config import cfg
from unix.use_system import unuse_system_libs, use_system_libs
from util.logger import logger

# Conditional import for Git library based on OS
if platform.system() == "Windows":
    import pygit2
    GitRepo = pygit2.Repository
else:
    from git import Repo as GitRepo  # Use GitPython on Unix

# This module is responsible for updating themes. It does not automatically do so, it is interfaced in the front-end.
class ThemeUpdater:

    def get_update_list(self, force: bool = False):
        if force:
            self.re_initialize()
        if not self.has_cache:
            self.process_updates()
            self.has_cache = True
        return json.dumps({"updates": self.update_list, "notifications": cfg.get_config()["updateNotifications"]})


    def set_update_notifs_status(self, status: bool):
        cfg.set_config_keypair("updateNotifications", status)
        return True


    def query_themes(self):
        themes = json.loads(find_all_themes())
        needs_copy = False
        update_queue = []

        for theme in themes:
            path = os.path.join(Millennium.steam_path(), "steamui", "skins", theme["native"])
            try:
                # Use the platform-specific Git repository class
                repo = GitRepo(path)
                self.update_query.append((theme, repo))
            except (pygit2.GitError if platform.system() == "Windows" else Exception):
                if "github" in theme["data"]:
                    needs_copy = True
                    update_queue.append((theme, path))
            except Exception as e:
                logger.log(f"An exception occurred: {e}")

        if needs_copy:
            source_dir = os.path.join(Millennium.steam_path(), "steamui", "skins")
            destination_dir = os.path.join(
                Millennium.steam_path(),
                "steamui",
                f"skins-backup-{datetime.now().strftime('%Y-%m-%d@%H-%M-%S')}"
            )
            if os.path.exists(destination_dir):
                shutil.rmtree(destination_dir)
            shutil.copytree(source_dir, destination_dir)

        for theme, path in update_queue:
            logger.log(f"upgrading theme to GIT @ {path}")
            if "github" in theme["data"]:
                self.pull_head(path, theme["data"]["github"])


    def construct_post_body(self):
        post_body = []
        for theme, repo in self.update_query:
            if "github" in theme.get("data", {}):
                github_data = theme["data"]["github"]
                owner = github_data.get("owner")
                repo = github_data.get("repo_name")
                if owner and repo:
                    post_body.append({'owner': owner, 'repo': repo})
        return post_body


    def pull_head(self, path: str, data: any) -> None:
        try:
            shutil.rmtree(path)
            repo_url = f'https://github.com/{data["owner"]}/{data["repo_name"]}.git'
            if platform.system() == "Windows":
                pygit2.clone_repository(repo_url, path)
            else:
                GitRepo.clone_from(repo_url, path)
        except Exception as e:
            logger.log(f"An exception occurred: {e}")


    def update_theme(self, native: str) -> bool:
        logger.log(f"updating theme {native}")
        try:
            path = os.path.join(Millennium.steam_path(), "steamui", "skins", native)
            repo = GitRepo(path)
            if platform.system() == "Windows":
                remote_name = 'origin'
                remote = repo.remotes[remote_name]
                # Enable automatic proxy detection
                remote.fetch(proxy = True)
                latest_commit = repo.revparse_single('origin/HEAD').id
                repo.reset(latest_commit, pygit2.GIT_RESET_HARD)
            else:
                use_system_libs()
                repo.remotes.origin.pull()
                unuse_system_libs()
        except Exception as e:
            logger.log(f"An exception occurred: {e}")
            return False
        self.re_initialize()
        return True


    def needs_update(self, remote_commit: str, theme: str, repo) -> bool:
        if platform.system() == "Windows":
            local_commit = repo[repo.head.target].id
        else:
            local_commit = repo.head.commit.hexsha
        return str(local_commit) != str(remote_commit)


    def check_theme(self, theme, repo_name, repo):
        remote = next((item for item in self.remote_json if item.get("name") == repo_name), None)
        if remote:
            if self.needs_update(remote['commit'], theme, repo):
                self.update_list.append({
                    'message': remote['message'],
                    'date': arrow.get(remote['date']).humanize(),
                    'commit': remote['url'],
                    'native': theme["native"],
                    'name': theme["data"].get("name", theme["native"])
                })


    def re_initialize(self):
        return self.__init__()


    def fetch_updates(self):
        self.update_list = []
        self.update_query = []
        self.query_themes()

        response = requests.post(
            "https://steambrew.app/api/v2/checkupdates",
            data=json.dumps(self.construct_post_body()),
            headers={"Content-Type": "application/json"}
        )
        if response.status_code != 200:
            logger.warn("An error occurred checking for updates!")
            return
        return response.json()


    def process_updates(self) -> bool:
        start_time = time.time()
        self.remote_json = self.fetch_updates()
        if self.remote_json:
            for theme, repo in self.update_query:
                if "data" in theme and "github" in theme["data"]:
                    repo_name = theme["data"]["github"].get("repo_name")
                    if repo_name:
                        self.check_theme(theme, repo_name, repo)
            if self.update_list:
                logger.log(f"found updates for {[theme['native'] for theme in self.update_list]} in {round((time.time() - start_time) * 1000, 4)} ms")


    def __init__(self):
        self.has_cache = False
