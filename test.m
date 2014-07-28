function test

% vol=nii_read_volume('Sub1_Anat_msk.nii');
% load response_predict.mat
% load V1CoorInT1Inter.mat
% load V1CoorInT1.mat

vol=double(nii_read_volume('Resliced_Sub1_Anat_msk.nii'));
load response_predict.mat
load V1CoorInT1NewDistr.mat

minval=min(label_predict(:));
maxval=max(label_predict(:));

for iter=1:size(label_predict,1)

l1=label_predict(iter,:);

coord=round(V1CoorInT1NewDistr);
dmap=zeros(size(vol));
for ii=1:length(l1)
dmap(coord(1,ii),coord(2,ii),coord(3,ii))=l1(ii)*600+1500;
end

%% display cummulated slice in XYZ coordinate
% subplot(131),imshow(squeeze(sum(dmap,1)),[]);
% subplot(132),imshow(squeeze(sum(dmap,2)),[]);
% subplot(133),imshow(squeeze(sum(dmap,3)),[]);

%% construct mha data
label=sprintf('%03d',iter);
mkdir(label);
volrender(vol,[pwd '/' label '/vol'],dmap);

%% run volume rendering program
env=getenv('LD_LIBRARY_PATH');
setenv('LD_LIBRARY_PATH','');
system([pwd '/volrender -MHA ' pwd '/' label '/vol.mha -Jet']);
setenv('LD_LIBRARY_PATH',env);

end

