namespace iiv_setting
{
    partial class FormSetting
    {
        /// <summary>
        ///  Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        ///  Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        ///  Required method for Designer support - do not modify
        ///  the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            txtViewer = new TextBox();
            btnBrowseViewer = new Button();
            SuspendLayout();
            // 
            // txtViewer
            // 
            txtViewer.Location = new Point(42, 43);
            txtViewer.Name = "txtViewer";
            txtViewer.Size = new Size(603, 23);
            txtViewer.TabIndex = 0;
            // 
            // btnBrowseViewer
            // 
            btnBrowseViewer.Location = new Point(651, 43);
            btnBrowseViewer.Name = "btnBrowseViewer";
            btnBrowseViewer.Size = new Size(36, 23);
            btnBrowseViewer.TabIndex = 1;
            btnBrowseViewer.Text = "&...";
            btnBrowseViewer.UseVisualStyleBackColor = true;
            btnBrowseViewer.Click += btnBrowseViewer_Click;
            // 
            // FormSetting
            // 
            AutoScaleDimensions = new SizeF(7F, 15F);
            AutoScaleMode = AutoScaleMode.Font;
            ClientSize = new Size(727, 384);
            Controls.Add(btnBrowseViewer);
            Controls.Add(txtViewer);
            Name = "FormSetting";
            Text = "iiv_setting";
            FormClosed += FormSetting_FormClosed;
            Load += FormSetting_Load;
            ResumeLayout(false);
            PerformLayout();
        }

        #endregion

        private TextBox txtViewer;
        private Button btnBrowseViewer;
    }
}
