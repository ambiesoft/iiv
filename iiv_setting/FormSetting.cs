namespace iiv_setting
{
    using Ambiesoft;
    public partial class FormSetting : Form
    {
        static readonly string SECTION_OPTION = "option";

        static readonly string KEY_VIEWER = "viewer";

        public FormSetting()
        {
            InitializeComponent();

            HashIni ini = Profile.ReadAll(IniPath);

            string sVal;
            Profile.GetString(SECTION_OPTION, KEY_VIEWER, "", out sVal, ini);
            txtViewer.Text = sVal;
        }

        string IniPath
        {
            get
            {
                return Path.Combine(
                    Application.StartupPath, "iiv.ini");
            }
        }
        private void FormSetting_Load(object sender, EventArgs e)
        {

        }

        private void FormSetting_FormClosed(object sender, FormClosedEventArgs e)
        {
            HashIni ini = Profile.ReadAll(IniPath);

            Profile.WriteString(SECTION_OPTION, KEY_VIEWER, txtViewer.Text, ini);

            if (!Profile.WriteAll(ini, IniPath))
            {
                MessageBox.Show(this,
                    "Failed to write ini file",
                    Application.ProductName,
                    MessageBoxButtons.OK,
                    MessageBoxIcon.Error);
            }
        }

        private void btnBrowseViewer_Click(object sender, EventArgs e)
        {
            OpenFileDialog ofd = new OpenFileDialog();
            ofd.Title = "Select viewer";
            ofd.Filter = "Executable files (*.exe)|*.exe|All files (*.*)|*.*";
            ofd.Multiselect = false;
            if(ofd.ShowDialog() != DialogResult.OK)
                return;
            txtViewer.Text = ofd.FileName;
        }
    }
}
