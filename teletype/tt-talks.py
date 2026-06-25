import yt_dlp

def download_subtitles(video_url):
    ydl_opts = {
        'write_subs': True,
        'write_auto_subs': True,
        'skip_download': True,
        'sub_format': 'srt',
        'outtmpl': '%(title)s.%(ext)s',
        # Highlight-Start
        # Automatically extracts cookies from your local browser instance
        # Change 'chrome' to 'firefox', 'edge', etc., depending on what you use
        'cookiesfrombrowser': ('chrome',), 
        # Highlight-End
    }

    with yt_dlp.YoutubeDL(ydl_opts) as ydl:
        ydl.download([video_url])

if __name__ == '__main__':
    video_url = "https://www.youtube.com/watch?v=mMAhjRKrpZE"
    download_subtitles(video_url)
