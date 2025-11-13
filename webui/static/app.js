(function(){
  const tabEncrypt = document.getElementById('tab-encrypt');
  const tabDecrypt = document.getElementById('tab-decrypt');
  const actionInput = document.getElementById('action');
  const dropzone = document.getElementById('dropzone');
  const filesInput = document.getElementById('files-input');
  const folderInput = document.getElementById('folder-input');
  const selectedList = document.getElementById('selected-list');
  const clearBtn = document.getElementById('clear-btn');
  const form = document.getElementById('process-form');
  const relpathsContainer = document.getElementById('relpaths-container');
  const passwordInput = document.getElementById('password');

  let files = [];

  function setAction(act){
    actionInput.value = act;
    tabEncrypt.classList.toggle('active', act==='encrypt');
    tabDecrypt.classList.toggle('active', act==='decrypt');
  }

  tabEncrypt.addEventListener('click', ()=> setAction('encrypt'));
  tabDecrypt.addEventListener('click', ()=> setAction('decrypt'));

  function addFiles(fileList, relBase){
    for(const f of fileList){
      const rel = f.webkitRelativePath && f.webkitRelativePath.length > 0 ? f.webkitRelativePath : f.name;
      files.push({file:f, relPath: rel});
    }
    renderSelected();
  }

  function renderSelected(){
    selectedList.innerHTML = '';
    files.forEach((item, idx)=>{
      const li = document.createElement('li');
      li.textContent = `${item.relPath} (${(item.file.size/1024).toFixed(1)} KB)`;
      const btn = document.createElement('button');
      btn.type = 'button';
      btn.textContent = 'Remove';
      btn.className = 'secondary';
      btn.style.marginLeft = '0.5rem';
      btn.onclick = ()=>{ files.splice(idx,1); renderSelected(); };
      li.appendChild(btn);
      selectedList.appendChild(li);
    });
  }

  // Helpers to traverse dropped folders (Chrome/Edge/Safari)
  async function traverseEntry(entry, pathPrefix=""){
    if(entry.isFile){
      const file = await new Promise((resolve, reject)=> entry.file(resolve, reject));
      const rel = pathPrefix ? `${pathPrefix}/${file.name}` : (file.webkitRelativePath || file.name);
      files.push({file, relPath: rel});
    } else if(entry.isDirectory){
      const reader = entry.createReader();
      async function readAll(){
        return await new Promise((resolve)=>{
          reader.readEntries(async (entries)=>{
            // readEntries may return empty array when done
            for(const ent of entries){
              await traverseEntry(ent, pathPrefix ? `${pathPrefix}/${entry.name}` : entry.name);
            }
            resolve(entries.length);
          });
        });
      }
      // Keep reading until no more entries
      while((await readAll()) > 0){}
    }
  }

  dropzone.addEventListener('dragover', (e)=>{ e.preventDefault(); dropzone.classList.add('hover'); });
  dropzone.addEventListener('dragleave', ()=> dropzone.classList.remove('hover'));
  dropzone.addEventListener('drop', async (e)=>{
    e.preventDefault(); dropzone.classList.remove('hover');
    const dt = e.dataTransfer;
    if(dt && dt.items && dt.items.length){
      const items = Array.from(dt.items);
      // Prefer using DataTransferItem to detect directories
      for(const it of items){
        const entry = it.webkitGetAsEntry ? it.webkitGetAsEntry() : (it.getAsEntry ? it.getAsEntry() : null);
        if(entry){
          await traverseEntry(entry, "");
        } else if(it.kind === 'file'){
          const f = it.getAsFile();
          if(f){ files.push({file:f, relPath: f.webkitRelativePath || f.name}); }
        }
      }
      renderSelected();
      return;
    }
    // Fallback: just use files list
    if(e.dataTransfer && e.dataTransfer.files){
      addFiles(e.dataTransfer.files);
    }
  });

  filesInput.addEventListener('change', ()=>{ addFiles(filesInput.files); filesInput.value=''; });
  folderInput.addEventListener('change', ()=>{ addFiles(folderInput.files); folderInput.value=''; });

  clearBtn.addEventListener('click', ()=>{ files=[]; renderSelected(); });

  form.addEventListener('submit', async (e)=>{
    e.preventDefault();
    const fd = new FormData();
    fd.append('action', actionInput.value || 'encrypt');
    fd.append('password', passwordInput.value || '');

    // Append files and relpaths in sequence so backend can match indexes if needed
    files.forEach(item => {
      fd.append('files', item.file, item.file.name);
      fd.append('relpaths', item.relPath);
    });

    try{
      const res = await fetch('/process', { method:'POST', body: fd });
      // If backend redirects, follow to results
      if(res.redirected){
        window.location.href = res.url;
        return;
      }
      // Fallback: try to parse JSON with a url
      const text = await res.text();
      try{ const data = JSON.parse(text); if(data.url){ window.location.href = data.url; return; } }catch{ /* ignore */ }
      alert('Upload failed or unexpected response.');
    }catch(err){
      console.error(err);
      alert('Error while uploading/processing files.');
    }
  });
})();
