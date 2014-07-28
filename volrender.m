function volrender(vol,fname,dmap)

if ~exist('fname'),fname='001';end
vol=double(vol);

fp=fopen('template.mha','rt');
template=fread(fp,10000,'char=>char');
fclose(fp);

mha=sprintf(template,size(vol,1),size(vol,2),size(vol,2),fname);

fp=fopen([fname '.mha'],'wt');
fwrite(fp,mha,'char');
fclose(fp);

minval=min(vol(:));
maxval=max(vol(:));
vol=(vol-minval)./(maxval-minval);
vol=(vol-1)*3000.0;

%hist(vol(:),60)
if exist('dmap')
% vol=dmap;
idx=find(dmap~=0);
vol(idx)=dmap(idx);
end

% idx=find(vol>0);
% hist(vol(idx),60)

fp=fopen([fname '.raw'],'wb');
fwrite(fp,vol,'short');
fclose(fp);

