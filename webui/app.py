from flask import Flask, render_template, request, send_file, send_from_directory, redirect, url_for, flash, abort
from werkzeug.utils import secure_filename
from pathlib import Path
import tempfile
import shutil
import subprocess
import uuid
import os
import json

app = Flask(__name__)
app.secret_key = "dev-secret-key"

PROJECT_ROOT = Path(__file__).resolve().parents[1]
BINARY_PATH = PROJECT_ROOT / "encrypt_decrypt"
ENV_PATH = PROJECT_ROOT / ".env"
JOBS_ROOT = Path(tempfile.gettempdir()) / "encrypty-ui-jobs"
JOBS_ROOT.mkdir(parents=True, exist_ok=True)

@app.route("/")
def index():
    return render_template("index.html")


def _save_uploaded_files(temp_dir: Path):
    saved_count = 0
    # Combine 'files' and 'folder' inputs
    uploads = []
    for field in ("files", "folder"):
        uploads.extend(request.files.getlist(field))

    # Attempt to capture relative paths passed from the browser for folder uploads
    relpaths = request.form.getlist("relpaths")  # optional, one per file in order

    for i, storage in enumerate(uploads):
        if not storage or storage.filename == "":
            continue
        filename = secure_filename(storage.filename)
        # Use provided relative path if available to preserve folder structure
        rel = relpaths[i] if i < len(relpaths) and relpaths[i] else filename
        # Fall back to filename only if rel path is blank
        rel_path = Path(rel)
        # Sanitize path components
        safe_parts = [secure_filename(p) for p in rel_path.parts]
        safe_rel_path = Path(*safe_parts)
        target_path = temp_dir / safe_rel_path
        target_path.parent.mkdir(parents=True, exist_ok=True)
        storage.save(target_path)
        saved_count += 1
    return saved_count


@app.route("/process", methods=["POST"]) 
def process():
    action = request.form.get("action", "encrypt").strip()
    password = request.form.get("password", "").strip()

    if action not in ("encrypt", "decrypt"):
        flash("Invalid action.")
        return redirect(url_for("index"))

    if not BINARY_PATH.exists():
        flash("encrypt_decrypt binary not found. Build the project with `make`." )
        return redirect(url_for("index"))

    # Create a job workspace
    job_id = f"job-{uuid.uuid4().hex[:8]}"
    work_root = JOBS_ROOT / job_id
    upload_dir = work_root / "uploads"
    upload_dir.mkdir(parents=True, exist_ok=True)

    try:
        count = _save_uploaded_files(upload_dir)
        if count == 0:
            shutil.rmtree(work_root, ignore_errors=True)
            flash("No files were uploaded.")
            return redirect(url_for("index"))

        # Call the existing binary against the upload directory (recursive)
        cmd = [str(BINARY_PATH), action, str(upload_dir)]
        if password:
            cmd.append(password)

        # Run with cwd at project root so .env is picked up and relative paths work
        proc = subprocess.run(cmd, cwd=str(PROJECT_ROOT), stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        log = proc.stdout
        (work_root / "process.log").write_text(log)

        # Build manifest of processed files (exclude .lock sidecars from listing)
        manifest = []
        for p in upload_dir.rglob('*'):
            if p.is_file():
                rel = p.relative_to(upload_dir).as_posix()
                if rel.endswith('.lock'):
                    continue
                try:
                    size = p.stat().st_size
                except OSError:
                    size = 0
                manifest.append({"path": rel, "size": size})
        (work_root / 'manifest.json').write_text(json.dumps({
            "action": action,
            "files": manifest
        }))

        return redirect(url_for('results', job_id=job_id), code=303)
    finally:
        # Cleanup handled via a separate retention policy; keeping job for downloads
        pass


@app.route('/results/<job_id>')
def results(job_id: str):
    work_root = JOBS_ROOT / job_id
    manifest_path = work_root / 'manifest.json'
    if not manifest_path.exists():
        abort(404)
    data = json.loads(manifest_path.read_text())
    return render_template('results.html', job_id=job_id, action=data.get('action'), files=data.get('files', []))


def _safe_join_upload(job_id: str, relpath: str) -> Path:
    base = JOBS_ROOT / job_id / 'uploads'
    target = (base / relpath).resolve()
    if not str(target).startswith(str(base.resolve())):
        abort(400)
    if not target.exists() or not target.is_file():
        abort(404)
    return target


@app.route('/download/<job_id>')
def download_file(job_id: str):
    relpath = request.args.get('path', '')
    if not relpath:
        abort(400)
    target = _safe_join_upload(job_id, relpath)
    return send_from_directory(directory=str(target.parent), path=target.name, as_attachment=True)


@app.route('/download-all/<job_id>')
def download_all(job_id: str):
    work_root = JOBS_ROOT / job_id
    upload_dir = work_root / 'uploads'
    if not upload_dir.exists():
        abort(404)
    zip_path = work_root / 'results.zip'
    if not zip_path.exists():
        # Create archive without including .lock files
        temp_stage = work_root / 'stage'
        if temp_stage.exists():
            shutil.rmtree(temp_stage, ignore_errors=True)
        shutil.copytree(upload_dir, temp_stage)
        # Remove .lock files from stage
        for p in temp_stage.rglob('*.lock'):
            p.unlink(missing_ok=True)
        shutil.make_archive(zip_path.with_suffix(''), 'zip', root_dir=temp_stage)
        shutil.rmtree(temp_stage, ignore_errors=True)
    return send_file(str(zip_path), as_attachment=True, download_name='processed.zip')


if __name__ == "__main__":
    # Host locally; for dev only
    app.run(host="127.0.0.1", port=5057, debug=True)
