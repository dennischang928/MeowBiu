from flask import Flask, render_template, request, send_file, jsonify
import os
import subprocess
from werkzeug.utils import secure_filename
import tempfile
import shutil

app = Flask(__name__)
app.config["MAX_CONTENT_LENGTH"] = 500 * 1024 * 1024  # 500MB max file size
app.config["UPLOAD_FOLDER"] = tempfile.mkdtemp()

ALLOWED_EXTENSIONS = {"mp4", "avi", "mov", "mkv", "webm"}


def allowed_file(filename):
    return "." in filename and filename.rsplit(".", 1)[1].lower() in ALLOWED_EXTENSIONS


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/process", methods=["POST"])
def process_videos():
    try:
        # Check if files are present
        if "video1" not in request.files or "video2" not in request.files:
            return jsonify({"error": "Both videos are required"}), 400

        video1 = request.files["video1"]
        video2 = request.files["video2"]

        if video1.filename == "" or video2.filename == "":
            return jsonify({"error": "No files selected"}), 400

        if not (allowed_file(video1.filename) and allowed_file(video2.filename)):
            return jsonify({"error": "Invalid file format"}), 400

        # Create temp directory for this request
        temp_dir = tempfile.mkdtemp()

        # Save uploaded files
        video1_path = os.path.join(temp_dir, "video1.mp4")
        video2_path = os.path.join(temp_dir, "video2.mp4")
        reversed_path = os.path.join(temp_dir, "reversed.mp4")
        concat_file = os.path.join(temp_dir, "concat.txt")
        output_path = os.path.join(temp_dir, "output.mp4")

        video1.save(video1_path)
        video2.save(video2_path)

        # Reverse video 1 using ffmpeg
        subprocess.run(
            [
                "ffmpeg",
                "-i",
                video1_path,
                "-vf",
                "reverse",
                "-af",
                "areverse",
                "-y",
                reversed_path,
            ],
            check=True,
            capture_output=True,
        )

        # Create concat file
        with open(concat_file, "w") as f:
            f.write(f"file '{reversed_path}'\n")
            f.write(f"file '{video2_path}'\n")

        # Concatenate videos
        subprocess.run(
            [
                "ffmpeg",
                "-f",
                "concat",
                "-safe",
                "0",
                "-i",
                concat_file,
                "-c",
                "copy",
                "-y",
                output_path,
            ],
            check=True,
            capture_output=True,
        )

        # Send the file
        return send_file(
            output_path,
            mimetype="video/mp4",
            as_attachment=True,
            download_name="reversed_video.mp4",
        )

    except subprocess.CalledProcessError as e:
        return jsonify({"error": f"FFmpeg error: {e.stderr.decode()}"}), 500
    except Exception as e:
        return jsonify({"error": str(e)}), 500
    finally:
        # Cleanup temp directory
        try:
            shutil.rmtree(temp_dir)
        except:
            pass


if __name__ == "__main__":
    print("Starting Video Reverser Web App...")
    print("Open your browser and go to: http://localhost:5000")
    app.run(debug=True, port=5000)
